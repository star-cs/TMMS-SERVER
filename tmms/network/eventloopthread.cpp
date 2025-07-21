#include "network/eventloopthread.h"
#include "network/eventloop.h"
#include <mutex>

namespace tmms::net
{

EventLoopThread::EventLoopThread() : thread_([this]() { startEventLoop(); })
{
}
EventLoopThread::~EventLoopThread()
{
    Run();
    if (loop_)
    {
        loop_->Quit();
    }

    if (thread_.joinable())
    {
        thread_.join();
    }
}

// 备注：Run 和 startEventLoop 在不同的线程
// 子线程 创建后 condition_ 等待 主线程 Run 启动
// 主线程 get_future 等待 子线程 开启事件循环

void EventLoopThread::Run()
{
    std::call_once(
        once_,
        [this]()
        {
            {
                std::lock_guard<std::mutex> lk(lock_);
                running_ = true;
                condition_.notify_all();
            }

            // 确保 已经 startEventLoop 过。
            auto f = promise_loop_.get_future();
            f.get();
        });
}

void EventLoopThread::startEventLoop()
{
    EventLoop                    loop; // 使用一个局部变量的原因是，想让loop的生命周期和线程是一致的
    std::unique_lock<std::mutex> lk(lock_);

    condition_.wait(lk, [this]() { return running_; });

    loop_ = &loop;

    promise_loop_.set_value(1);

    loop.loop(); // 启动事件循环

    loop_ = nullptr;
}

} // namespace tmms::net