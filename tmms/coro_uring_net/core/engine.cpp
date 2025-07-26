#include "coro_uring_net/core/engine.hpp"
#include "base/config/config.h"
#include "coro_uring_net/core/task.hpp"
#include "coro_uring_net/io/io_info.hpp"
#include <coroutine>

namespace tmms::net
{
auto engine::init() noexcept -> void
{
    linfo.egn            = this;
    m_num_io_wait_submit = 0;
    m_num_io_running     = 0;
    m_upxy.init(config::kEntryLength);
}

auto engine::deinit() noexcept -> void
{
    m_upxy.deinit();
    m_num_io_wait_submit = 0;
    m_num_io_running     = 0;
    base::AtomicQueue<std::coroutine_handle<>> task_queue;
    m_task_queue.swap(task_queue);
}

auto engine::ready() noexcept -> bool
{
    return m_task_queue.was_size() > 0;
}

auto engine::get_free_urs() noexcept -> ursptr
{
    return m_upxy.get_free_sqe();
}

auto engine::num_task_schedule() noexcept -> size_t
{
    return m_task_queue.was_size();
}

auto engine::schedule() noexcept -> std::coroutine_handle<>
{
    auto coro = m_task_queue.pop();
    assert(coro != nullptr);
    return coro;
}

auto engine::submit_task(std::coroutine_handle<> handle) noexcept -> void
{
    assert(handle != nullptr && "engine get nullptr task handle");
    m_task_queue.push(handle); // atomic_queue 第三方库操作是线程安全的
    // 来了新任务，向 eventfd 写入 task_flag
    wake_up();
}

auto engine::exec_one_task() noexcept -> void
{
    auto coro = schedule();
    coro.resume();
    if (coro.done())
    {
        clean(coro);
    }
}

auto engine::handle_cqe_entry(urcptr cqe) noexcept -> void
{
    auto data = reinterpret_cast<io_info*>(io_uring_cqe_get_data(cqe));
    /**
    io_uring_cqe 结构体常用的 变量
    user_data
        用户自定义数据，关联提交与完成事件的核心标识。用户在提交 SQE（提交队列条目）时通过
        io_uring_sqe_set_data 设置的任意 64 位值（如指针或标识符），内核在返回 CQE 时原样回传此值
    cb
        提交 SQE（提交队列条目）时通过 设置的回调
    res
        记录 I/O 操作的执行结果，其含义随操作类型动态变化
        读操作（readv）	实际读取的字节数	负错误码（如 -EINVAL）
        写操作（writev）	实际写入的字节数	负错误码（如 -ENOSPC）
        接受连接（accept）	新 socket 的文件描述符	负错误码（如 -ECONNABORTED）
        定时器（timeout）	超时前完成的任务数	-ETIME（超时）或 -EINVAL

    */
    // 备注：这里会默认调用 设置的cb回调，把结果返回出去
    data->cb(data, cqe->res);
}

auto engine::poll_submit() noexcept -> void
{
    // 对 IO 任务 提交
    do_io_submit();

    // 等待 I/O 执行
    // 工作线程在 无任何任务 的情况下 利用阻塞在 eventfd 读操作上来让出执行权防止 CPU 空转。
    auto cnt = m_upxy.wait_eventfd();
    if (!wake_by_cqe(cnt))
    {
        return;
    }

    // 取出 I/O
    int num = m_upxy.peek_batch_cqe(m_urc.data(), config::kQueCap);

    if (num != 0)
    {
        for (int i = 0; i < num; i++)
        {
            handle_cqe_entry(m_urc[i]);
        }
        m_upxy.cq_advance(num);
        m_num_io_running -= num;
    }
}

auto engine::wake_up(uint64_t val) noexcept -> void
{
    m_upxy.write_eventfd(val);
}

auto engine::add_io_submit() noexcept -> void
{
    m_num_io_wait_submit += 1;
}

auto engine::empty_io() noexcept -> bool
{
    return m_num_io_wait_submit == 0 && m_num_io_running == 0;
}

auto engine::do_io_submit() noexcept -> void
{
    // 利用条件判断来决定是否需要调用 io_uring 的提交操作
    if (m_num_io_wait_submit > 0)
    {
        m_upxy.submit();
        m_num_io_running += m_num_io_wait_submit;
        m_num_io_wait_submit = 0; // io_uring 会一次提交所有 IO
    }
}

} // namespace tmms::net