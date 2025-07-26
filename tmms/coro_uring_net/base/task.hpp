/*
 * @Author: star-cs
 * @Date: 2025-07-26 10:25:29
 * @LastEditTime: 2025-07-26 13:14:07
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/base/task.hpp
 * @Description:
 */
#pragma once

#include "base/config/config.h"
#include "base/macro.h"
#include "coro_uring_net/base/container.hpp"
#include "meta_info.hpp"
#include <coroutine>
#include <cstdint>

namespace tmms::net
{
template<typename return_type = void>
class task;

enum class coro_state : uint8_t
{
    normal,
    detach,
    none
};

struct promise_base
{
    friend struct final_awaitable;
    struct final_awaitable
    {
        /**
        注：
        initial_suspend() 或 final_suspend() 可返回 noop_coroutine 的等待器，实现：
        ​立即启动​（initial_suspend 返回 noop 时协程不挂起）。
        ​自动销毁​（final_suspend 返回 noop 时协程帧自动释放）。
        */

        constexpr auto await_ready() const noexcept -> bool { return false; }

        // 子协程结束后，返回父协程句柄
        template<typename promise_type>
        auto await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept -> std::coroutine_handle<>
        {
            // coroutine子协程
            auto& promise = coroutine.promise();
            // 如果没有记录父协程（即没有 co_await 创建子协程），那么就返回 std::noop_coroutine()（见上方的注）
            return promise.m_continuation != nullptr ? promise.m_continuation : std::noop_coroutine();
        }
        constexpr auto await_resume() noexcept -> void {}
    };
    promise_base() noexcept = default;
    ~promise_base()         = default;

    // task<> 初始化暂停
    constexpr auto initial_suspend() noexcept { return std::suspend_always{}; }

    // 协程结束运行调用
    auto final_suspend() noexcept
    {
        // 备注：每个调度点返回，会隐形调用 co_await awaitable(返回值)
        return final_awaitable{}; // 也就是在子协程上下文执行 co_await final_awaitable，目的返回子协程的父协程句柄
    }

    auto continuation(std::coroutine_handle<> continuation) noexcept -> void { m_continuation = continuation; }

    auto set_state(coro_state state) -> void { m_state = state; }

    auto get_state() -> coro_state { return m_state; }

    auto is_detach() -> bool { return m_state == coro_state::detach; }
#ifdef ENABLE_MEMORY_ALLOC
    void* operator new(std::size_t size) { return tmms::meta::ginfo.mem_alloc->allocate(size); }

    void operator delete(void* ptr, std::size_t size) { tmms::meta::ginfo.mem_alloc->release(ptr); }
#endif

protected:
    std::coroutine_handle<> m_continuation{nullptr};     // 保存当前的父协程句柄
    coro_state              m_state{coro_state::normal}; // 当前子协程的状态
};

template<typename return_type>
struct promise final : public promise_base, public container<return_type>
{
public:
    using task_type        = task<return_type>;
    using coroutine_handle = std::coroutine_handle<promise<return_type>>;

#ifdef DEBUG
    template<typename... Args>
    promise(int id, Args&&... args) noexcept
    {
        promise_id = id;
    }
#endif // DEBUG
    promise() noexcept {}
    promise(const promise&)             = delete;
    promise(promise&& other)            = delete;
    promise& operator=(const promise&)  = delete;
    promise& operator=(promise&& other) = delete;
    ~promise()                          = default;

    //
    auto get_return_object() noexcept -> task_type;

    // co_return 返回数值
    // container<return_type> 里面实现了 ~
    // constexpr auto return_value(return_type result) { m_result = result; }

    auto unhandled_exception() noexcept -> void { this->set_exception(); }
};

template<>
struct promise<void> : public promise_base
{
    using task_type        = task<void>;
    using coroutine_handle = std::coroutine_handle<promise<void>>;

#ifdef DEBUG
    template<typename... Args>
    promise(int id, Args&&... args) noexcept
    {
        promise_id = id;
    }
#endif // DEBUG
    promise() noexcept                  = default;
    promise(const promise&)             = delete;
    promise(promise&& other)            = delete;
    promise& operator=(const promise&)  = delete;
    promise& operator=(promise&& other) = delete;
    ~promise()                          = default;

    // 初始化 task<>
    auto get_return_object() noexcept -> task_type;

    constexpr auto return_void() noexcept -> void {}

    auto unhandled_exception() noexcept -> void { m_exception_ptr = std::current_exception(); }

    auto result() -> void
    {
        if (m_exception_ptr)
        {
            std::rethrow_exception(m_exception_ptr);
        }
    }

private:
    std::exception_ptr m_exception_ptr{nullptr};
};

template<typename return_type>
class task
{
public:
    using task_type        = task<return_type>;
    using promise_type     = net::promise<return_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    struct awaitable_base
    {
        awaitable_base(coroutine_handle coroutine) noexcept : m_coroutine(coroutine) {}

        auto await_ready() const noexcept -> bool { return !m_coroutine || m_coroutine.done(); }

        auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> std::coroutine_handle<>
        {
            // TODO[lab1]: Add you codes
            // 传入的 awaiting_coroutine 是 co_wait fun（子协程结束或suspend后）父协程的句柄
            m_coroutine.promise().continuation(awaiting_coroutine);
            return m_coroutine;
        }
        // struct awaitable继承后，co_await()里{}初始化已经赋值了，当前tast的句柄
        std::coroutine_handle<promise_type> m_coroutine{nullptr};
    };

    task() noexcept : m_coroutine(nullptr) {}

    explicit task(coroutine_handle handle) : m_coroutine(handle) {}
    task(const task&) = delete;
    task(task&& other) noexcept : m_coroutine(std::exchange(other.m_coroutine, nullptr)) {}

    ~task()
    {
        if (m_coroutine != nullptr)
        {
            m_coroutine.destroy();
        }
    }

    auto operator=(const task&) -> task& = delete;

    auto operator=(task&& other) noexcept -> task&
    {
        if (std::addressof(other) != this)
        {
            if (m_coroutine != nullptr)
            {
                m_coroutine.destroy();
            }

            m_coroutine = std::exchange(other.m_coroutine, nullptr);
        }

        return *this;
    }

    /**
     * @return True if the task is in its final suspend or if the task has been destroyed.
     */
    auto is_ready() const noexcept -> bool { return m_coroutine == nullptr || m_coroutine.done(); }

    auto resume() -> bool
    {
        if (!m_coroutine.done())
        {
            m_coroutine.resume();
        }
        return !m_coroutine.done();
    }

    auto destroy() -> bool
    {
        if (m_coroutine != nullptr)
        {
            m_coroutine.destroy();
            m_coroutine = nullptr;
            return true;
        }

        return false;
    }

    auto detach() -> void
    {
        // TODO[lab1]: Add you codes
        CORE_ASSERT2(m_coroutine != nullptr, "detach func expected no-nullptr coroutine_handler");
        auto& promise = m_coroutine.promise();
        promise.set_state(coro_state::detach);
        m_coroutine = nullptr; // 放弃所有权。通过外部调用 clean 清除
    }

    // 1. 调用 co_await xx。xx 子协程执行完后，生成 co_await awaitable
    auto operator co_await() const& noexcept
    {
        struct awaitable : public awaitable_base
        {
            // 获取到 协程内 co_return 的返回值
            auto await_resume() -> decltype(auto) { return this->m_coroutine.promise().result(); }
        };
        // 把当前句柄传入
        return awaitable{m_coroutine};
    }

    auto operator co_await() const&& noexcept
    {
        struct awaitable : public awaitable_base
        {
            auto await_resume() -> decltype(auto) { return std::move(this->m_coroutine.promise()).result(); }
        };

        return awaitable{m_coroutine};
    }

    auto promise() & -> promise_type& { return m_coroutine.promise(); }
    auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
    auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

    auto handle() & -> coroutine_handle { return m_coroutine; }
    auto handle() && -> coroutine_handle { return std::exchange(m_coroutine, nullptr); }

private:
    coroutine_handle m_coroutine{nullptr};
};

using coroutine_handle = std::coroutine_handle<net::promise_base>;

inline auto clean(std::coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab1]: Add you codes
    // 手动清除 句柄
    // std::coroutine_handle<detail::promise_base>::from_address(void*)
    // 接受一个内存地址，返回指向detail::promise_base类型协程的句柄
    auto  specific_handle = coroutine_handle::from_address(handle.address());
    auto& promise         = specific_handle.promise();
    switch (promise.get_state())
    {
        case coro_state::detach:
            handle.destroy();
            break;
        default:
            break;
    }
}

template<typename return_type>
inline auto promise<return_type>::get_return_object() noexcept -> task<return_type>
{
    return task<return_type>{coroutine_handle::from_promise(*this)};
}

inline auto promise<void>::get_return_object() noexcept -> task<>
{
    return task<>{coroutine_handle::from_promise(*this)};
}

} // namespace tmms::net