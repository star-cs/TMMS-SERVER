/*
 * @Author: star-cs
 * @Date: 2025-07-26 16:57:46
 * @LastEditTime: 2025-07-26 21:24:38
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/core/scheduler.hpp
 * @Description:
 */
#pragma once

#include "base/atomic_helper.hpp"
#include "base/config/config.h"
#include "coro_uring_net/base/dispatcher.hpp"
#include "coro_uring_net/base/meta_info.hpp"
#include "coro_uring_net/core/context.hpp"

namespace tmms::net
{
class scheduler
{
    friend context;

public:
    inline static auto init(size_t ctx_cnt = std::thread::hardware_concurrency()) noexcept -> void
    {
        if (ctx_cnt == 0)
        {
            ctx_cnt = std::thread::hardware_concurrency();
        }
        get_instance()->init_impl(ctx_cnt);
    }

    /**
     * @brief loop work mode, auto wait all context finish job
     *
     */
    inline static auto loop() noexcept -> void { get_instance()->loop_impl(); }

    static inline auto submit(task<void>&& task) noexcept -> void
    {
        auto handle = task.handle();
        task.detach();
        submit(handle);
    }

    static inline auto submit(task<void>& task) noexcept -> void { submit(task.handle()); }

    inline static auto submit(std::coroutine_handle<> handle) noexcept -> void
    {
        get_instance()->submit_task_impl(handle);
    }

private:
    static auto get_instance() noexcept -> scheduler*
    {
        static scheduler sc;
        return &sc;
    }

    auto init_impl(size_t ctx_cnt) noexcept -> void;

    auto start_impl() noexcept -> void;

    auto loop_impl() noexcept -> void;

    auto stop_impl() noexcept -> void;

    auto submit_task_impl(std::coroutine_handle<> handle) noexcept -> void;

private:
    size_t                                m_ctx_cnt{0};
    std::vector<std::unique_ptr<context>> m_ctxs;
    dispatcher<config::kDispatchStrategy> m_dispatcher;
    std::vector<base::atomic_ref_wrapper<int>>
                     m_ctx_stop_flag; // 存储各个 context 的执行状态，每个上下文对应的状态。0无任务完成任务，1有任务
    std::atomic<int> m_stop_token;    // 引用计数成员变量，0表示所有上下文完成所有任务，调度器可以停止了

#ifdef ENABLE_MEMORY_ALLOC
                                   // Memory Allocator
    tmms::base::memory_allocator<config::kMemoryAllocator> m_mem_alloc;
#endif
};

inline void submit_to_scheduler(task<void>&& task) noexcept
{
    scheduler::submit(std::move(task));
}

inline void submit_to_scheduler(task<void>& task) noexcept
{
    scheduler::submit(task.handle());
}

inline void submit_to_scheduler(std::coroutine_handle<> handle) noexcept
{
    scheduler::submit(handle);
}

} // namespace tmms::net