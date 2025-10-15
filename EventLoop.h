#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channal;
class Poller;

// 事件循环类，主要包含 Channal 和 Poller（epoll的抽象) 两个模块
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    // 唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法=>Poller的方法
    void updateChannal(Channal *channal);
    void removeChannal(Channal *channal);
    bool hasChannal(Channal *channal);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();       // 处理weakup
    void doPendingFunctor(); // 执行回调

    using ChannalList = std::vector<Channal *>;

    std::atomic_bool looping_;
    std::atomic_bool quit_; // 标志退出loop循环

    const pid_t threadId_; // 记录当前loop所在的线程id

    Timestamp pollReturnTime_; // poller返回发生事件的channals的时间点
    std::unique_ptr<Poller> poller_;

    /*
     * 主要作用，当mainloop获取一个新用户的channal，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理
     * 作用方式：每个eventloop的poller都监听一个自己的wakefd，
     * 通过向wakefd写入数据，触发对应eventloop的poller，结束其epoll_wait阻塞，进而处理可能的回调操作或quit
     */
    int wakeupFd_;
    std::unique_ptr<Channal> wakeupChannal_;

    ChannalList activeChannals_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                        // 互斥锁，用来保护上面vector容器的线程安全操作
};