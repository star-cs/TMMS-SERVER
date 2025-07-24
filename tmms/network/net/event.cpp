#include "network/net/event.h"
#include "base/log/log.h"
#include "network/net/eventloop.h"
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace tmms::net
{
Event::Event() : fd_(-1), loop_(nullptr)
{
}
Event::Event(EventLoop* loop) : loop_(loop)
{
}
Event::Event(EventLoop* loop, int fd) : fd_(fd), loop_(loop)
{
}
Event::~Event()
{
    Close();
}

void Event::OnRead()
{
}
void Event::OnWrite()
{
}
void Event::OnClose()
{
}
void Event::OnError(const std::string& msg)
{
}

bool Event::EnableWriting(bool enable)
{
    return loop_->EnableEventWriting(shared_from_this(), enable);
}
bool Event::EnableReading(bool enable)
{
    return loop_->EnableEventReading(shared_from_this(), enable);
}

void Event::Close()
{
    if (fd_ > 0)
    {
        close(fd_);
        fd_ = -1;
    }
}

PipeEvent::PipeEvent(EventLoop* loop) : Event(loop)
{
    int fd[2] = {
        0,
    };
    auto ret = ::pipe2(fd, O_NONBLOCK);
    if (ret < 0)
    {
        CORE_ERROR("pope open failed.");
        exit(-1);
    }

    fd_       = fd[0]; // 读端
    write_fd_ = fd[1]; // 写端
}

PipeEvent::~PipeEvent()
{
    if (write_fd_ > 0)
    {
        ::close(write_fd_);
        write_fd_ = -1;
    }
}
void PipeEvent::OnRead()
{
    int64_t tmp; // 默认 读64位数据
    auto    ret = ::read(fd_, &tmp, sizeof(tmp));
    if (ret < 0)
    {
        CORE_ERROR("pipe read error.error:{}", errno);
    }
    CORE_DEBUG("pipe read:{}", tmp);
}

void PipeEvent::OnClose()
{
    if (write_fd_ > 0)
    {
        ::close(write_fd_);
        write_fd_ = -1;
    }
}

void PipeEvent::OnError(const std::string& msg)
{
    std::cout << "error : " << msg << std::endl;
}

void PipeEvent::Write(const char* data, size_t len)
{
    ::write(write_fd_, data, len);
}

} // namespace tmms::net