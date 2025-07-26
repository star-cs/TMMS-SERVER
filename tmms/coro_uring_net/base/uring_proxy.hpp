/*
 * @Author: star-cs
 * @Date: 2025-07-26 15:11:21
 * @LastEditTime: 2025-07-26 15:27:48
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/base/uring_proxy.hpp
 * @Description: 对liburing封装
 */
#pragma once
#include "base/config/config.h"
#include "base/log/log.h"
#include "base/utils/utils.h"
#include <liburing.h>
#include <sys/eventfd.h>
#include "coro_uring_net/base/marked_buffer.hpp"

namespace tmms::net
{
using ursptr     = io_uring_sqe*;
using urcptr     = io_uring_cqe*;
using urchandler = std::function<void(urcptr)>;

using uring_fds_item = marked_buffer<int, config::kFixFdArraySize>::item;

inline constexpr uring_fds_item invalid_fd_item = uring_fds_item{.idx = -1, .ptr = nullptr};

class uring_proxy
{
public:
    uring_proxy() noexcept
    {
        // must init in construct func
        m_efd = eventfd(0, 0);
        if (m_efd < 0)
        {
            CORE_ERROR("uring_proxy init event_fd failed");
            std::exit(1);
        }
    }

    ~uring_proxy() noexcept = default;

    auto init(unsigned int entry_length) noexcept -> void
    {
        // don't need to set m_para
        memset(&m_para, 0, sizeof(m_para));

#ifdef ENABLE_SQPOOL
        m_para.flags |= IORING_SETUP_SQPOLL;
        m_para.sq_thread_idle = config::kSqthreadIdle;
#endif // ENABLE_SQPOOL

        auto res = io_uring_queue_init_params(entry_length, &m_uring, &m_para);
        if (res != 0)
        {
            CORE_ERROR("uring_proxy init uring failed");
            std::exit(1);
        }

        res = io_uring_register_eventfd(&m_uring, m_efd);
        if (res != 0)
        {
            CORE_ERROR("uring_proxy bind event_fd to uring failed");
            std::exit(1);
        }

        if constexpr (config::kEnableFixfd)
        {
            m_fds.init();

            // prepare nullfd
            m_null_fds = std::vector<int>{};
            for (int i = 0; i < config::kFixFdArraySize; i++)
            {
                auto fd = base::get_null_fd();
                if (fd <= 0)
                {
                    CORE_ERROR("open null fd failed");
                    std::exit(1);
                }
                m_null_fds.push_back(fd);
            }

            m_fds.set_data(m_null_fds);

            res = io_uring_register_files(&m_uring, m_fds.data, config::kFixFdArraySize);
            if (res != 0)
            {
                CORE_ERROR("uring_proxy register files failed");
                std::exit(1);
            }
        }
    }

    auto deinit() noexcept -> void
    {
        // this operation cost too much time, so don't call this function
        // io_uring_unregister_eventfd(&m_uring);
        close(m_efd);
        m_efd = -1;

        if constexpr (config::kEnableFixfd)
        {
            for (auto fd : m_null_fds)
            {
                close(fd);
            }
        }

        io_uring_queue_exit(&m_uring);
    }

    /**
     * @brief return if uring has finished io
     *
     * @note no-block function
     *
     * @return true
     * @return false
     */
    auto peek_uring() noexcept -> bool
    {
        urcptr cqe{nullptr};
        io_uring_peek_cqe(&m_uring, &cqe);
        return cqe != nullptr;
    }

    /**
     * @brief wait the number of finished io
     *
     * @note block function
     *
     * @param num
     */
    auto wait_uring(int num = 1) noexcept -> void
    {
        urcptr cqe;
        if (num == 1) [[likely]]
        {
            io_uring_wait_cqe(&m_uring, &cqe);
        }
        else
        {
            io_uring_wait_cqe_nr(&m_uring, &cqe, num);
        }
    }

    /**
     * @brief mark the cqe entry has beed processed
     *
     * @param cqe
     */
    inline auto seen_cqe_entry(urcptr cqe) noexcept -> void { io_uring_cqe_seen(&m_uring, cqe); }

    /**
     * @brief get the free uring sqe 获取空闲的sqe
     *
     * @return ursptr
     */
    inline auto get_free_sqe() noexcept -> ursptr { return io_uring_get_sqe(&m_uring); }

    /**
     * @brief submit all sqe entry and return the number of submitted sqe entry
     *
     * @return int
     */
    inline auto submit() noexcept -> int { return io_uring_submit(&m_uring); }

    /**
     * @brief use io_uring_for_each_cqe to process cqe entry
     *
     * @param f
     * @param mark_finish
     * @return size_t
     */
    auto handle_for_each_cqe(urchandler f, bool mark_finish = false) noexcept -> size_t
    {
        urcptr   cqe;
        unsigned head;
        unsigned i = 0;
        io_uring_for_each_cqe(&m_uring, head, cqe)
        {
            f(cqe);
            i++;
        };
        if (mark_finish)
        {
            cq_advance(i);
        }
        return i;
    }

    auto wait_eventfd() noexcept -> uint64_t
    {
        uint64_t u;
        auto     ret = eventfd_read(m_efd, &u);
        assert(ret != -1 && "eventfd read error");
        return u;
    }

    /**
     * @brief batch fetch cqe entry
     *
     * @param cqes
     * @param num
     */
    inline auto peek_batch_cqe(urcptr* cqes, unsigned int num) noexcept -> int
    {
        return io_uring_peek_batch_cqe(&m_uring, cqes, num);
    }

    inline auto write_eventfd(uint64_t num) noexcept -> void 
    {
        auto ret = eventfd_write(m_efd, num);
        assert(ret != -1 && "eventfd write error");
    }

    /**
     * @brief an efficient way of seen cqe entry
     *
     * @param num
     */
    inline auto cq_advance(unsigned int num) noexcept -> void { io_uring_cq_advance(&m_uring, num); }

    /**
     * @brief Get one fixed fd
     *
     * @return uring_fds_item, if fds is empty, uring_fds_item.index will be less than 0
     */
    auto get_fixed_fd() noexcept -> uring_fds_item
    {
        if constexpr (!config::kEnableFixfd)
        {
            return invalid_fd_item;
        }
        return m_fds.borrow();
    }

    /**
     * @brief return back fixed fd
     *
     * @param item
     */
    auto back_fixed_fd(uring_fds_item item) noexcept -> void
    {
        if (!item.valid())
        {
            return;
        }
        m_fds.data[item.idx] = m_null_fds[item.idx];
        update_register_fixed_fds(item.idx); // change back to null fd
        m_fds.return_back(item);
    }

    auto update_register_fixed_fds(int index) noexcept -> void
    {
        if constexpr (config::kEnableFixfd)
        {
            // TODO: Why local update is incorrect?
            // io_uring_register_files_update(&m_uring, index, m_fds.data, 1)

            auto res = io_uring_register_files_update(&m_uring, 0, m_fds.data, config::kFixFdArraySize);
            if (res != config::kFixFdArraySize)
            {
                CORE_ERROR("update register files failed, result: {}", res);
                std::exit(1);
            }
        }
    }

private:
    int             m_efd{0};
    io_uring_params m_para;
    io_uring        m_uring;

    // Use m_fds to utilize the IOSQE_FIXED_FILE feature of io_uring
    std::vector<int> m_null_fds;

    // 对象池 预分配
    marked_buffer<int, config::kFixFdArraySize> m_fds;
};

} // namespace tmms::net