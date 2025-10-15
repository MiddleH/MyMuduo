#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channal;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannalList = std::vector<Channal *>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannalList *activeChannals) = 0;
    virtual void updateChannal(Channal *channal) = 0;
    virtual void removeChannal(Channal *channal) = 0;

    // 判断channal是否在当前的Poller中
    bool hasChannal(Channal *channal) const;

    // EventLoop通过该接口获取IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // map的key：sockfd  value：sockfd所属的channal
    using ChannalMap = std::unordered_map<int, Channal *>;
    ChannalMap channals_;

private:
    EventLoop *ownerLoop_; // 定义Poller所属得到事件循环EventLoop
};