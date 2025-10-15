#include "Channal.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

// 事件标识
const int Channal::kNoneEvent = 0;
const int Channal::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channal::kWriteEvent = EPOLLOUT;

Channal::Channal(EventLoop *loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
{
}

Channal::~Channal()
{
}

void Channal::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/*
 * 改变channal所表示的fd的eventS事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 */
void Channal::
    update()
{
    // 通过channal所属的EventLoop。调用poller的相应方法，注册fd的events事件
    loop_->updateChannal(this);
}

void Channal::remove()
{
    loop_->removeChannal(this);
}

void Channal::handleEvent(Timestamp recviceTime)
{
    LOG_INFO("handleEvent(Timestamp recviceTime)1");
    if (tied_)
    {
        LOG_INFO("handleEvent(Timestamp recviceTime)2");
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            LOG_INFO("handleEvent(Timestamp recviceTime)3");
            handleEventWithGuard(recviceTime);
        }
    }
    else
    {
        handleEventWithGuard(recviceTime);
    }
}

// 根据poller通知的channal发生的具体事件，channal调用相应的回调操作
void Channal::handleEventWithGuard(Timestamp recviceTime)
{
    LOG_INFO("channal handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & EPOLLIN)
    {
        if (readCallback_)
        {
            readCallback_(recviceTime);
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}