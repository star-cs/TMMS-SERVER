#pragma once

#include "base/noncopyable.h"
#include "network/eventloop.h"
#include <condition_variable>
#include <future>
#include <thread>

namespace tmms::net
{
class EventLoopThread : public base::NonCopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();

    void         Run();
    std::thread& Thread() { return thread_; }
    EventLoop*   Loop() const { return loop_; }

private:
    void startEventLoop();

private:
    EventLoop*              loop_{nullptr};
    std::thread             thread_;
    bool                    running_{false};
    std::mutex              lock_;
    std::condition_variable condition_;
    std::once_flag          once_; // 只运行一次
    std::promise<int>       promise_loop_;
};
} // namespace tmms::net