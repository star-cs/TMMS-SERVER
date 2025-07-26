/*
 * @Author: star-cs
 * @Date: 2025-07-26 20:02:51
 * @LastEditTime: 2025-07-26 20:05:54
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/io/io_awaitable.hpp
 * @Description:
 */
#pragma once

#include "coro_uring_net/io/base_awaitable.hpp"
#include <netinet/in.h>
namespace tmms::net
{
namespace io
{
class noop_awaiter : public base_io_awaiter
{
public:
    noop_awaiter() noexcept;
    static auto callback(io_info* data, int res) noexcept -> void;
};

class stdin_awaiter : public base_io_awaiter
{
public:
    stdin_awaiter(char* buf, size_t len, int io_flag = 0, int sqe_flag = 0) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;
};
} // namespace io

namespace tcp
{
class tcp_accept_awaiter : public base_io_awaiter
{
public:
    tcp_accept_awaiter(int listenfd, int io_flag = 0, int sqe_flag = 0) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;

private:
    inline static socklen_t len = sizeof(sockaddr_in);
};

class tcp_read_awaiter : public base_io_awaiter
{
public:
    tcp_read_awaiter(int sockfd, char* buf, size_t len, int io_flag = 0, int sqe_flag = 0) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;
};

class tcp_write_awaiter : public base_io_awaiter
{
public:
    tcp_write_awaiter(int sockfd, char* buf, size_t len, int io_flag = 0, int sqe_flag = 0) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;
};

class tcp_close_awaiter : public base_io_awaiter
{
public:
    tcp_close_awaiter(int sockfd) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;
};

class tcp_connect_awaiter : public base_io_awaiter
{
public:
    tcp_connect_awaiter(int sockfd, const sockaddr* addr, socklen_t addrlen) noexcept;

    static auto callback(io_info* data, int res) noexcept -> void;
};
}; // namespace tcp

} // namespace tmms::net