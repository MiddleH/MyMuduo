#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>
#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress addr, const std::string name)
        : loop_(loop),
          server_(loop, addr, name)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConncetion, this, std::placeholders::_1));
        // 注册读写消息回调函数
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this,std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop的线程数
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }
private:
    // 连接建立或断开的回调
    void onConncetion(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("connection UP : %s \n", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("connection DOWN : %s \n", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp recviceTime)
    {
        std::string msg = buffer->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop,addr,"EchoServer");

    server.start();
    loop.loop();

    return 0;
}