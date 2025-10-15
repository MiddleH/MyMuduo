#pragma once

/*
*   继承noncopyable的派生类无法进行拷贝构造和赋值操作，可以正常构造和析构
*/ 
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};