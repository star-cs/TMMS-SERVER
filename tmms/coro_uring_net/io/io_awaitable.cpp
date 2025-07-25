/*
 * @Author: star-cs
 * @Date: 2025-07-26 20:06:24
 * @LastEditTime: 2025-07-26 21:53:43
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/io/io_awaitable.cpp
 * @Description:
 */
#include "coro_uring_net/io/io_awaitable.hpp"
#include "coro_uring_net/core/engine.hpp"
#include "coro_uring_net/core/scheduler.hpp"
#include "coro_uring_net/io/io_info.hpp"
#include <liburing.h>
#include <unistd.h>

namespace tmms::net
{
namespace io
{
noop_awaiter::noop_awaiter() noexcept
{
    m_info.type = io_type::nop;
    m_info.cb   = &noop_awaiter::callback;

    io_uring_prep_nop(m_urs);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto noop_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res; // 在 io_info 中设置返回值，之后在 io awaiter 的 await_resume 函数中返回

    // 向当前上下文绑定的 context 提交协程句柄来恢复等待 io 的协程，
    // 在长期运行模式下也可以调用 submit_to_scheduler
    submit_to_context(data->handle);
}

stdin_awaiter::stdin_awaiter(char* buf, size_t len, int io_flag, int sqe_flag) noexcept
{
    m_info.type = io_type::stdin;
    m_info.cb   = &stdin_awaiter::callback;
    io_uring_sqe_set_flags(m_urs, sqe_flag);
    io_uring_prep_read(m_urs, STDIN_FILENO, buf, len, io_flag);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto stdin_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res;
    submit_to_context(data->handle);
}

}; // namespace io

namespace tcp
{
tcp_accept_awaiter::tcp_accept_awaiter(int listenfd, int io_flag, int sqe_flag) noexcept
{
    m_info.type = io_type::tcp_accept;
    m_info.cb   = &tcp_accept_awaiter::callback;

    // base_awaitable 是 线程内的engine，所有对 m_urs 的操作是线程安全的
    io_uring_sqe_set_flags(m_urs, sqe_flag);
    io_uring_prep_accept(m_urs, listenfd, nullptr, &len, io_flag);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto tcp_accept_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res;
    submit_to_context(data->handle);
}

tcp_read_awaiter::tcp_read_awaiter(int sockfd, char* buf, size_t len, int io_flag, int sqe_flag) noexcept
{
    m_info.type = io_type::tcp_read;
    m_info.cb   = &tcp_read_awaiter::callback;

    io_uring_sqe_set_flags(m_urs, sqe_flag);
    io_uring_prep_recv(m_urs, sockfd, buf, len, io_flag);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto tcp_read_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res;
    submit_to_context(data->handle);
}

tcp_write_awaiter::tcp_write_awaiter(int sockfd, char* buf, size_t len, int io_flag, int sqe_flag) noexcept
{
    m_info.type = io_type::tcp_write;
    m_info.cb   = &tcp_write_awaiter::callback;

    io_uring_sqe_set_flags(m_urs, sqe_flag);
    io_uring_prep_send(m_urs, sockfd, buf, len, io_flag);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto tcp_write_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res;
    submit_to_context(data->handle);
}

tcp_close_awaiter::tcp_close_awaiter(int sockfd) noexcept
{
    m_info.type = io_type::tcp_close;
    m_info.cb   = &tcp_close_awaiter::callback;

    io_uring_prep_close(m_urs, sockfd);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto tcp_close_awaiter::callback(io_info* data, int res) noexcept -> void
{
    data->result = res;
    submit_to_context(data->handle);
}

tcp_connect_awaiter::tcp_connect_awaiter(int sockfd, const sockaddr* addr, socklen_t addrlen) noexcept
{
    m_info.type = io_type::tcp_connect;
    m_info.cb   = &tcp_connect_awaiter::callback;
    m_info.data = CASTDATA(sockfd);

    io_uring_prep_connect(m_urs, sockfd, addr, addrlen);
    io_uring_sqe_set_data(m_urs, &m_info);
    local_engine().add_io_submit();
}

auto tcp_connect_awaiter::callback(io_info* data, int res) noexcept -> void
{
    if (res != 0)
    {
        data->result = res;
    }
    else
    {
        data->result = static_cast<int>(data->data);
    }
    submit_to_context(data->handle);
}

}; // namespace tcp
}; // namespace tmms::net