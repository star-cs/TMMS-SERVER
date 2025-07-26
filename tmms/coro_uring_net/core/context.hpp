/*
 * @Author: star-cs
 * @Date: 2025-07-26 16:10:57
 * @LastEditTime: 2025-07-26 20:12:37
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/core/context.hpp
 * @Description:
 */
#pragma once

#include "coro_uring_net/core/engine.hpp"
#include "coro_uring_net/core/task.hpp"
#include <functional>

namespace tmms::net
{
class scheduler;

/**
 * @brief Each context own one engine, it's the core part of tinycoro,
 * which can process computation task and io task
 */
class context
{
    using stop_cb = std::function<void()>;

public:
    context() noexcept;
    ~context() noexcept                = default;
    context(const context&)            = delete;
    context(context&&)                 = delete;
    context& operator=(const context&) = delete;
    context& operator=(context&&)      = delete;

    auto init() noexcept -> void;

    auto deinit() noexcept -> void;
    /**
     * @brief work thread start running
     *
     */
    auto start() noexcept -> void;

    /**
     * @brief send stop signal to work thread
     *
     */
    auto notify_stop() noexcept -> void;

    /**
     * @brief wait work thread stop
     *
     */
    inline auto join() noexcept -> void { m_job->join(); }

    inline auto submit_task(task<void>&& task) noexcept -> void
    {
        // 传递右值，意味外部失去所有权，所以 task.detach();
        auto handle = task.handle();
        task.detach();
        this->submit_task(handle);
    }

    inline auto submit_task(task<void>& task) noexcept -> void { submit_task(task.handle()); }

    /**
     * @brief submit one task handle to context
     * @param handle
     */
    auto submit_task(std::coroutine_handle<> handle) noexcept -> void;

    /**
     * @brief get context unique id
     *
     * @return ctx_id
     */
    inline auto get_ctx_id() noexcept -> config::ctx_id { return m_id; }

    /**
     * @brief add reference count of context
     *
     * @param register_cnt
     */
    auto register_wait(int register_cnt = 1) noexcept -> void;

    /**
     * @brief reduce reference count of context
     *
     * @param register_cnt
     */
    auto unregister_wait(int register_cnt = 1) noexcept -> void;

    inline auto get_engine() noexcept -> engine& { return m_engine; }

    /**
     * @brief main logic of work thread
     *
     * @param token
     */
    auto run(std::stop_token token) noexcept -> void;

    auto set_stop_cb(stop_cb cb) noexcept -> void { m_stop_cb = cb; }

    // 驱动 engine 从任务队列取出任务并执行
    auto process_work() noexcept -> void;

    // 驱动 engine 执行 IO 任务
    auto poll_work() noexcept -> void { m_engine.poll_submit(); }

    // 判断是否没有 IO 任务以及引用计数是否为 0
    auto empty_wait_task() noexcept -> bool
    {
        return m_num_wait_task.load(std::memory_order_acquire) == 0 && m_engine.empty_io();
    }

private:
    alignas(config::kCacheLineSize) engine m_engine;
    std::unique_ptr<std::jthread> m_job;
    config::ctx_id                m_id;

    std::atomic<size_t> m_num_wait_task{0};
    stop_cb             m_stop_cb; // context 完成所有任务后应该执行的停止逻辑
};

inline context& local_context() noexcept
{
    return *linfo.ctx;
}

inline void submit_to_context(task<void>&& task) noexcept
{
    local_context().submit_task(std::move(task));
}

inline void submit_to_context(task<void>& task) noexcept
{
    local_context().submit_task(task.handle());
}

inline void submit_to_context(std::coroutine_handle<> handle) noexcept
{
    local_context().submit_task(handle);
}

} // namespace tmms::net