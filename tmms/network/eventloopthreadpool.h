#pragma once

#include "base/noncopyable.h"
#include "network/eventloop.h"
#include "network/eventloopthread.h"
#include <memory>

namespace tmms::net
{

class EventLoopThreadPool : public base::NonCopyable
{
public:
    EventLoopThreadPool(int thread_num, int start = 0, int cpus = 4);
    ~EventLoopThreadPool();

    std::vector<EventLoop*> GetLoops() const;
    EventLoop*              GetNextLoop();
    size_t                  Size();
    void                    Start();

private:
    std::vector<std::shared_ptr<EventLoopThread>> threads_;       // 线程池
    std::atomic_int32_t                           loop_index_{0}; // 当前取到的loop index,多线程并发
};

} // namespace tmms::net