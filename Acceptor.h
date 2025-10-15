#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channal.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    bool listenning()const{return listenning_;}
    void listen();
private:
    void handleRead();

    EventLoop *loop_; // 用户定义的baseloop/mainloop
    Socket acceptSocket_;//监听新连接消息的fd
    Channal acceptChannal_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};