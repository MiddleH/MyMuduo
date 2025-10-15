#include "EpollPoller.h"
#include "Channal.h"
#include "Logger.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

// channal的成员变量index_的值
// channal未添加到poller
const int kNew = -1;
// channal已添加到poller
const int kAdded = 1;
// channal从poller中删除
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vevtor<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}

// 重写基类Poller的抽象方法
Timestamp EpollPoller::poll(int timeoutMs, ChannalList *activeChannals)
{
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channals_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannals(numEvents, activeChannals);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll() err!");
        }
    }
    LOG_INFO("poll(int timeoutMs, ChannalList *activeChannals)");
    return now;
}

// channal update/remove => EventLoop updateChannal/removeChannal => Poller updateChannal/removeChannal
/*
 *           EventLoop
 *   ChannalList     Poller
 *                ChannalMap<int,Channal*>
 */
void EpollPoller::updateChannal(Channal *channal)
{
    const int index = channal->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channal->fd(), channal->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channal->fd();
            channals_[fd] = channal;
        }

        channal->set_index(kAdded);
        update(EPOLL_CTL_ADD, channal);
    }
    else // channal已经在poller上注册过了
    {
        int fd = channal->fd();
        if (channal->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channal);
            channal->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channal);
        }
    }
}

void EpollPoller::removeChannal(Channal *channal)
{
    int fd = channal->fd();
    channals_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channal->index();

    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channal);
    }
    channal->set_index(kNew);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannals(int numEvents, ChannalList *activeChannals) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channal *channal = static_cast<Channal *>(events_[i].data.ptr);
        channal->set_revents(events_[i].events);
        activeChannals->push_back(channal); // EventLoop得到它的poller返回的发生事件的channal列表
    }
}

// 更新channal通道 epoll_ctl add/mod/del
void EpollPoller::update(int operation, Channal *channal)
{
    epoll_event event;
    bzero(&event, sizeof(event));
    event.events = channal->events();
    int fd = channal->fd();
    event.data.fd = fd;
    event.data.ptr = channal;
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctr delete error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctr add/mod erron:%d\n", errno);
        }
    }
}