# MyMuduo
muduo服务端网络库的c++11实现

主要特点：

事件循环（EventLoop）：每个线程一个事件循环，用于处理I/O事件和定时器事件。

通道（Channel）：封装了文件描述符和感兴趣的事件，以及事件回调函数。

多线程模型：支持多线程，有一个主线程（MainLoop）和多个子线程（SubLoop），通过轮询方式将连接分发给子线程。

非阻塞I/O：使用epoll作为事件分发机制，所有I/O操作都是非阻塞的。

线程池：处理计算密集型任务，避免阻塞I/O线程。

核心组件：

EventLoop：事件循环，负责管理Channel和Pollor（epoll的封装）。

Channel：每个Channel负责一个文件描述符（如socket）的事件分发，将不同的时间设置为不同的回调函数（如读回调、写回调、错误回调等）。

Poller：封装epoll，是EventLoop的成员，用于监听文件描述符的事件，并将事件分发给对应的Channel。

Acceptor：用于接受新连接，并将新连接分发给EventLoop线程。

TcpConnection：封装已建立的TCP连接，包含输入输出缓冲区，处理连接建立、数据读写、连接断开等。

TcpServer：封装服务器，包含Acceptor和多个TcpConnection，管理连接的生命周期。

EventLoopThreadPool：事件循环线程池，管理多个EventLoop线程，用于实现多线程高并发。


本项目采用c++11实现muduo网络库的服务端部分，解耦原muduo网络库对boost库的依赖，致力于学习muduo网络库的优秀核心设计理念


编译方式：

直接调用根目录下的autobuild.sh脚本文件

使用方式：

与原muduo库保持一致

