/*
 * @Author: star-cs
 * @Date: 2025-07-26 16:20:30
 * @LastEditTime: 2025-07-26 20:50:59
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/core/context.cpp
 * @Description:
 */
#include "coro_uring_net/core/context.hpp"
#include <atomic>
#include <liburing.h>
#include <memory>
#include <stop_token>

namespace tmms::net
{
context::context() noexcept
{
    m_id = ginfo.context_id.fetch_add(1, std::memory_order_relaxed);
}

auto context::init() noexcept -> void
{
    linfo.ctx = this;
    m_engine.init();
}

auto context::deinit() noexcept -> void
{
    linfo.ctx = nullptr;
    m_engine.deinit();
}

auto context::start() noexcept -> void
{
    // 每个 context 在独立的线程里运行
    m_job = std::make_unique<std::jthread>(
        [this](std::stop_token token)
        {
            this->init();
            // 如果外部没有注入 stop_cb，那么自行为其添加逻辑
            if (!(this->m_stop_cb))
            {
                m_stop_cb = [&]() { m_job->request_stop(); };
            }
            this->run(token);
            this->deinit();
        });
}

auto context::notify_stop() noexcept -> void
{
    m_job->request_stop();
    m_engine.wake_up(); // 唤醒engin处理任务
}

auto context::submit_task(std::coroutine_handle<> handle) noexcept -> void
{
    m_engine.submit_task(handle);
}

auto context::register_wait(int register_cnt) noexcept -> void
{
    m_num_wait_task.fetch_add(register_cnt, std::memory_order_acq_rel);
}

auto context::unregister_wait(int register_cnt) noexcept -> void
{
    m_num_wait_task.fetch_sub(register_cnt, std::memory_order_acq_rel);
}

auto context::run(std::stop_token token) noexcept -> void
{
    // 在循环中驱动 engine 执行完所有任务并优雅退出
    while (!token.stop_requested())
    {
        // 处理engine所有 协程 任务
        process_work();

        if (empty_wait_task())
        {
            if (!m_engine.ready())
            {
                // 此处表明 contetx 已执行完所有任务，那么调用停止逻辑
                m_stop_cb();
            }
            else
            {
                continue;
            }
        }

        // 提交engine所有 I/O 任务，在eventfd处等待
        poll_work();
    }
}

auto context::process_work() noexcept -> void
{
    auto num = m_engine.num_task_schedule();
    for (int i = 0; i < num; ++i)
    {
        m_engine.exec_one_task();
    }
}

} // namespace tmms::net