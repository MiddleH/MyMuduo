#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

/*
* epoll的使用
* epoll_create
* epoll_ctl     add/mod/del
* epoll_wait
*/

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannalList *activeChannals) override;
    void updateChannal(Channal *channal) override;
    void removeChannal(Channal *channal) override;

private:
    static const int kInitEventListSize=16;

    //填写活跃的连接
    void fillActiveChannals(int numEvents,ChannalList *activeChannals)const;
    //更新channal通道
    void update(int operation,Channal *channal);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};