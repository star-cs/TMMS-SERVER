/*
 * @Author: star-cs
 * @Date: 2025-07-26 22:18:54
 * @LastEditTime: 2025-07-26 22:19:06
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/io/net/tcp/tcp.cpp
 * @Description:
 */
#include <cstdlib>

#include "coro_uring_net/io/net/tcp/tcp.hpp"
#include "base/log/log.h"

namespace tmms::net::tcp
{
tcp_server::tcp_server(const char* addr, int port) noexcept
{
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd != -1);

    base::set_fd_noblock(m_listenfd);

    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port   = htons(port);
    if (addr != nullptr)
    {
        if (inet_pton(AF_INET, addr, &m_servaddr.sin_addr.s_addr) < 0)
        {
            CORE_ERROR("addr invalid");
            std::exit(1);
        }
    }
    else
    {
        m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(m_listenfd, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) != 0)
    {
        CORE_ERROR("server bind error");
        std::exit(1);
    }

    if (listen(m_listenfd, config::kBacklog) != 0)
    {
        CORE_ERROR("server listen error");
        std::exit(1);
    }

    m_sqe_flag = 0;
    m_fixed_fd.assign(m_listenfd, m_sqe_flag);
}

tcp_accept_awaiter tcp_server::accept(int io_flags) noexcept
{
    return tcp_accept_awaiter(m_listenfd, io_flags, m_sqe_flag);
}

tcp_client::tcp_client(const char* addr, int port) noexcept
{
    m_clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_clientfd < 0)
    {
        CORE_ERROR("clientfd init error");
        std::exit(1);
    }

    base::set_fd_noblock(m_clientfd);

    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port   = htons(port);
    if (addr != nullptr)
    {
        if (inet_pton(AF_INET, addr, &m_servaddr.sin_addr.s_addr) < 0)
        {
            CORE_ERROR("address error");
            std::exit(1);
        }
    }
    else
    {
        m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

tcp_connect_awaiter tcp_client::connect() noexcept
{
    return tcp_connect_awaiter(m_clientfd, (sockaddr*)&m_servaddr, sizeof(m_servaddr));
}

}; // namespace tmms::net::tcp
