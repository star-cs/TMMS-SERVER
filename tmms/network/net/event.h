/*
 * @Author: star-cs
 * @Date: 2025-07-21 16:00:04
 * @LastEditTime: 2025-07-23 17:16:02
 * @FilePath: /TMMS-SERVER/tmms/network/net/event.h
 * @Description:
 */
#pragma once

#include <memory>
#include <sys/epoll.h>
namespace tmms::net
{
class EventLoop;
const int kEventRead  = (EPOLLIN | EPOLLPRI | EPOLLET); // 方便调用操作,读，紧急，边沿触发事件
const int kEventWrite = (EPOLLOUT | EPOLLET);

class Event : public std::enable_shared_from_this<Event>
{
    friend class EventLoop;

public:
    Event();
    Event(EventLoop* loop);
    Event(EventLoop* loop, int fd);
    virtual ~Event();

    virtual void OnRead();
    virtual void OnWrite();
    virtual void OnClose();
    virtual void OnError(const std::string& msg);

    bool EnableWriting(bool enable);
    bool EnableReading(bool enable);
    int  Fd() const { return fd_; }
    void Close();

protected:
    int        fd_{-1};
    EventLoop* loop_{nullptr}; // 在哪个loop中
    int        event_{0};      // 什么事件 EPOLLIN，一定要初始化成0，初始化为-1就出错了
};

class PipeEvent : public Event
{
public:
    PipeEvent(EventLoop* loop);
    ~PipeEvent();

    void OnRead() override;
    void OnClose() override;
    void OnError(const std::string& msg) override;
    // 区别在于 OnRead() 是读事件满足后读，Write是直接写
    void Write(const char* data, size_t len);

private:
    int write_fd_{-1};
};

} // namespace tmms::net