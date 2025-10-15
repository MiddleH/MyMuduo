#include "TcpConnection.h"
#include "Logger.h"
#include "EventLoop.h"
#include "Channal.h"
#include "Socket.h"

#include <functional>
#include <memory>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string nameArg, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channal_(new Channal(loop_, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64 M
{

     if (!socket_) {
        LOG_FATAL("Failed to create Socket for connection %s", name_.c_str());
        return;
    }
    
    if (!channal_) {
        LOG_FATAL("Failed to create Channel for connection %s", name_.c_str());
        return;
    }
    
    // 验证channel的有效性
    if (channal_->fd() != sockfd) {
        LOG_ERROR("Channel fd mismatch: expected %d, got %d", 
                 sockfd, channal_->fd());
    }

    // 给客户端fd对应channal设置相应回调，poller监听到事件发生时通知channal，channal调用相应回调进行处理
    channal_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channal_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channal_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    channal_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channal_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/*
 * 发送数据  应用写数据快，内核发送数据慢，需将数据写入发送缓冲区，且设置水位回调
 */
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 调用过该Connection的shutdown，不能进行发送了
    if (state_ == kDisConnected)
    {
        LOG_ERROR("disconnected,give up writing!");
        return;
    }

    // channal刚开始写数据，且发送缓冲区没有待发送数据
    if (!channal_->isWriting() && outPutBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channal_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            // 一次性写完，无需向注册epollout事件
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    // 一次write没有把数据全部发送出去，剩余的数据需要保存到TcpConnection发送缓冲区，并给channal注册epollout事件
    // 在TcpConnection发送缓冲区的数据发送完之前，poller在tcp发送缓冲区可写时会一直通知channal调用handlewrite方法
    // ，直到TcpConnection发送缓冲区数据为空
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outPutBuffer_.readableBytes();

        // 目前发送缓冲区剩余的待发送数据的长度加上当前剩余要发送的数据高过水位线
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }

        outPutBuffer_.append((char *)data + nwrote, remaining);

        if (!channal_->isWriting())
        {
            channal_->enableWriting(); // 注册写事件，使channal能够调用handlewrite
        }
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channal_->tie(shared_from_this());
    channal_->enableReading(); // 向poller注册读事件/epollin

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisConnected);
        channal_->disableAll(); // 从poller中将channal感兴趣的所有事件delete掉
        connectionCallback_(shared_from_this());
    }
    channal_->remove();
}

// 关闭连接
void TcpConnection::shutdown()
{
    if(state_==kConnected)
    {
        setState(kDisConnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}

// 关闭连接
void TcpConnection::shutdownInLoop()
{
    if(!channal_->isWriting())//channal的发送缓冲区的数据已经发送完成
    {
        socket_->shutdownWrite();//关闭写端
    }
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedError = 0;
    ssize_t n = inputBuffer_.readFd(channal_->fd(), &savedError);
    if (n > 0)
    {
        // 已建立连接的客户端，发生可读事件，调用用户传入的回调操作
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedError;
        LOG_ERROR("TcpConnectioon::handleRead err:%d\n", errno);
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if (channal_->isWriting())
    {
        int savedError = 0;
        ssize_t n = outPutBuffer_.writeFd(channal_->fd(), &savedError);

        if (n > 0)
        {

            outPutBuffer_.retrieve(n);
            if (outPutBuffer_.readableBytes() == 0)
            {
                channal_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ = kDisConnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handlWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing\n", channal_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose() fd=%d state=%d\n", channal_->fd(), (int)state_);
    setState(kDisConnected);
    channal_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());

    // 执行连接关闭的回调
    if (connectionCallback_)
    {
        connectionCallback_(connPtr);
    }
    // 执行关闭连接的回调
    if (closeCallback_)
    {
        closeCallback_(connPtr);
    }
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channal_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError() name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}