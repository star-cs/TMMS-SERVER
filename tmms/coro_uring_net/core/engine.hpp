#pragma once

#include "base/atomic_queue.hpp"
#include "base/config/config.h"
#include "coro_uring_net/base/meta_info.hpp"
#include "coro_uring_net/base/uring_proxy.hpp"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <liburing.h>
namespace tmms::net
{

#define wake_by_task(val) (((val) & engine::task_mask) > 0)
#define wake_by_io(val)   (((val) & engine::io_mask) > 0)
#define wake_by_cqe(val)  (((val) & engine::cqe_mask) > 0)

class engine
{
    friend class context;

public:
    // 约定 eventfd 读出的 数字 对应实际发生的情况
    static constexpr uint64_t task_mask = (0xFFFFF00000000000);
    static constexpr uint64_t io_mask   = (0x00000FFFFF000000);
    static constexpr uint64_t cqe_mask  = (0x0000000000FFFFFF); // liburing完成任务后，往eventfd写入1

    static constexpr uint64_t task_flag = (((uint64_t)1) << 44);
    static constexpr uint64_t io_flag   = (((uint64_t)1) << 24);

    engine() noexcept { m_id = ginfo.engine_id.fetch_add(1, std::memory_order_relaxed); }
    ~engine() noexcept = default;

    // forbidden to copy and move
    engine(const engine&)                    = delete;
    engine(engine&&)                         = delete;
    auto operator=(const engine&) -> engine& = delete;
    auto operator=(engine&&) -> engine&      = delete;

    auto init() noexcept -> void;

    auto deinit() noexcept -> void;

    /**
     * @brief return if engine has task to run
     *
     * @return true
     * @return false
     */
    auto ready() noexcept -> bool;

    /**
     * @brief get the free uring sqe entry
     *
     * @return ursptr
     */
    auto get_free_urs() noexcept -> ursptr;

    /**
     * @brief return number of task to run in engine
     *
     * @return size_t
     */
    auto num_task_schedule() noexcept -> size_t;

    /**
     * @brief fetch one task handle and engine should erase it from its queue
     *
     * @return coroutine_handle<>
     */
    auto schedule() noexcept -> std::coroutine_handle<>;

    /**
     * 对外调用 提交任务
     * @brief submit one task handle to engine
     *
     * @param handle
     */
    auto submit_task(std::coroutine_handle<> handle) noexcept -> void;

    /**
     * @brief this will call schedule() to fetch one task handle and run it
     *
     * @note this function will call clean() when handle is done
     */
    auto exec_one_task() noexcept -> void;

    /**
     * @brief handle one cqe entry
     *
     * @param cqe io_uring cqe entry
     */
    auto handle_cqe_entry(urcptr cqe) noexcept -> void;

    /**
     * @brief submit uring sqe and wait uring finish, then handle
     * cqe entry by call handle_cqe_entry
     * 最关键
     */
    auto poll_submit() noexcept -> void;

    /**
     * @brief write flag to eventfd to wake up thread blocked by read eventfd
     *
     * @param val
     */
    auto wake_up(uint64_t val = engine::task_flag) noexcept -> void;

    /**
     * @brief tell engine there has one io to be submitted, engine will record this
     *
     */
    auto add_io_submit() noexcept -> void;

    /**
     * @brief return if there has any io to be processed,
     * io include two types: submit_io, running_io
     *
     * @note submit_io: io to be submitted
     * @note running_io: io is submitted but it has't run finished
     *
     * @return true
     * @return false
     */
    auto empty_io() noexcept -> bool;

    /**
     * @brief return engine unique id
     *
     * @return uint32_t
     */
    inline auto get_id() noexcept -> uint32_t { return m_id; }

    /**
     * @brief Get the uring object owned by engine, so the outside can use
     * some function of uring
     *
     * @return uring_proxy&
     */
    inline auto get_uring() noexcept -> uring_proxy& { return m_upxy; }

private:
    // 提交io
    auto do_io_submit() noexcept -> void;

private:
    uint32_t    m_id;
    uring_proxy m_upxy;

    // store task handle
    // 非 IO 任务，协程任务
    base::AtomicQueue<std::coroutine_handle<>> m_task_queue; // You can replace it with another data structure

    // used to fetch cqe entry
    // 取出完成块
    std::array<io_uring_cqe*, config::kQueCap> m_urc;

    size_t m_num_io_wait_submit{0}; // 当前待提交的 IO 数
    size_t m_num_io_running{0};     // 正在运行且未完成的 IO 数
};

/**
 * @brief return local thread engine
 *
 * @return engine&
 */
inline engine& local_engine() noexcept
{
    return *linfo.egn;
}

} // namespace tmms::net