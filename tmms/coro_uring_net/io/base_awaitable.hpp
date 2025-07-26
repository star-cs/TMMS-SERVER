/*
 * @Author: star-cs
 * @Date: 2025-07-26 20:02:38
 * @LastEditTime: 2025-07-26 21:50:41
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/io/base_awaitable.hpp
 * @Description:
 */
#pragma once

#include "coro_uring_net/base/uring_proxy.hpp"
#include "coro_uring_net/core/engine.hpp"
#include "coro_uring_net/io/io_info.hpp"

namespace tmms::net
{
class base_io_awaiter
{
public:
    base_io_awaiter() noexcept : m_urs(local_engine().get_free_urs())
    {
        // TODO: you should no-block wait when get_free_urs return nullptr,
        // this means io submit rate is too high.
        assert(m_urs != nullptr && "io submit rate is too high");
    }

    // 暂停
    constexpr auto await_ready() noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> handle) noexcept -> void { m_info.handle = handle; }

    auto await_resume() noexcept -> int32_t { return m_info.result; }

protected:
    io_info m_info;
    ursptr  m_urs;
};

} // namespace tmms::net