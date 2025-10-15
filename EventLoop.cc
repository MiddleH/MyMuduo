#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channal.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller io复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subreactor处理新channal
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannal_(new Channal(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop create %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        // 该线程已包含一个EventLoop
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupChannal的事件类型及其回调操作
    wakeupChannal_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannal的EPOLLIN事件
    wakeupChannal_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannal_->disableAll();
    wakeupChannal_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_)
    {
        activeChannals_.clear();
        // 监听两类fd clientfd、wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannals_);

        for (Channal *channal : activeChannals_)
        {
            channal->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop需要处理的回调操作
        /*
         * IO线程 mainloop accept fd <<= channal -> subloop
         * mainloop 实现注册一个回调cb(subloop执行)      wakeup subloop后，执行下面的方法，执行之前mainloop注册的回调
         */
        doPendingFunctor();
    }

    LOG_INFO("EventLoop %p stop looping\n", this);
    looping_ = false;
}

// 退出事件循环
void EventLoop::quit()
{
    // eventloop在自己的线程中执行quit，则线程一定未阻塞在loop中
    quit_ = true; // 使线程不会进入loop中

    if (!isInLoopThread())
    {
        // eventloop的quit调用不在自己的线程中，是外部线程调用的eventloop的quit，则当前eventloop可能阻塞在loop中，需唤醒该eventloop跳出loop
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前loop线程中，执行cb
    {
        cb();
    }
    else // 不在当前loop线程中执行cb，需唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的需要执行cb回调操作的loop的线程
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

// 唤醒loop所在的线程  向wakeupfd写一个数据,wakeupchannal就会发生读事件，使eventloop解除处于poll的阻塞
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup(0 writes %lu bytes instead of 8\n)", n);
    }
}

// EventLoop的方法=>Poller的方法
void EventLoop::updateChannal(Channal *channal)
{
    poller_->updateChannal(channal);
}

void EventLoop::removeChannal(Channal *channal)
{
    poller_->removeChannal(channal);
}

bool EventLoop::hasChannal(Channal *channal)
{
    return poller_->hasChannal(channal);
}

// 执行回调
void EventLoop::doPendingFunctor()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    // 执行当前loop需要执行的回调操作
    for (const Functor &functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;
}