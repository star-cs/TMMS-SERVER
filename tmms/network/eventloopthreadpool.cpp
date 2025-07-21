#include "network/eventloopthreadpool.h"
#include "network/eventloop.h"
#include "network/eventloopthread.h"
#include <memory>
#include <pthread.h>
#include <vector>

namespace tmms::net
{
namespace
{
void bind_cpu(std::thread& t, int n)
{
    cpu_set_t cpu; // cpu_set_t 是一个位掩码，表示可以在哪些CPU上运行

    CPU_ZERO(&cpu);   //  初始化 cpu_set_t 变量
    CPU_SET(n, &cpu); // 将指定的CPU添加到 cpu_set_t 中。绑定第几个cpu，这里还没绑定，设定了mask

    // 设置线程亲和性可以将线程绑定到特定的CPU核心上运行，这在优化性能和控制多线程应用程序的行为时很有用。
    // t.native_handle()获取与 std::thread 对象关联的底层线程句柄。pthread_t类型对象
    pthread_setaffinity_np(t.native_handle(), sizeof(cpu), &cpu); // 设置线程的CPU亲和性。
}

} // namespace

EventLoopThreadPool::EventLoopThreadPool(int thread_num, int start, int cpus)
{
    if (thread_num <= 0)
    {
        thread_num = 1;
    }

    for (int i = 0; i < thread_num; ++i)
    {
        threads_.emplace_back(std::make_shared<EventLoopThread>());
        if (cpus > 0)
        {
            int n = (start + i) % cpus;
            bind_cpu(threads_.back()->Thread(), n);
        }
    }
}
EventLoopThreadPool::~EventLoopThreadPool()
{
}

/// @brief 线程不和外界交互，一切接口都是使用loop交互，loop是和eventLoop生命周期相同的
/// @return 返回所有线程的eventloop
std::vector<EventLoop*> EventLoopThreadPool::GetLoops() const
{
    std::vector<EventLoop*> result;
    for (auto& t : threads_)
    {
        result.emplace_back(t->Loop());
    }
    return result;
}

/// @brief 返回当前使用的EventLoop
/// @return  返回当前使用的EventLoop
EventLoop* EventLoopThreadPool::GetNextLoop()
{
    int index = loop_index_;
    loop_index_++;
    return threads_[index % threads_.size()]->Loop();
}

size_t EventLoopThreadPool::Size()
{
    return threads_.size();
}

void EventLoopThreadPool::Start()
{
    for (auto& t : threads_)
    {
        t->Run();
    }
}

} // namespace tmms::net