#include "Buffer.h"

#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

// 从fd上读取数据
ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536] = {0};

    struct iovec vec[2];

    const size_t writable = writeableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n < writable) // Buffer的可写缓冲区已经足够存储读出的数据
    {
        writerIndex_ += n;
    }
    else // extrabuf中也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *savedErrno = errno;
    }
    return n;
}