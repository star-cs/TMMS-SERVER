#include <algorithm>
#include <thread>
#include <tuple>
#include <vector>

#include "base/config/config.h"
#include "coro_uring_net/core/engine.hpp"
#include "coro_uring_net/core/task.hpp"
#include "coro_uring_net/io/io_info.hpp"
#include "gtest/gtest.h"

using namespace tmms::net;
using namespace tmms::base;
using namespace tmms;

/*************************************************************
 *                       pre-definition                      *
 *************************************************************/

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class EngineTest : public ::testing::Test
{
protected:
    void SetUp() override { m_engine.init(); }

    void TearDown() override { m_engine.deinit(); }

    engine           m_engine;
    std::vector<int> m_vec; // use vector to verify test result
};

class EngineNopIOTest : public ::testing::TestWithParam<int>
{
protected:
    void SetUp() override { m_engine.init(); }

    void TearDown() override { m_engine.deinit(); }

    engine               m_engine;
    std::vector<int>     m_vec;
    std::vector<io_info> m_infos;
};

class EngineMultiThreadTaskTest : public EngineNopIOTest
{
};

class EngineMixTaskNopIOTest : public ::testing::TestWithParam<std::tuple<int, int>>
{
protected:
    void SetUp() override { m_engine.init(); }

    void TearDown() override { m_engine.deinit(); }

    engine               m_engine;
    std::vector<int>     m_vec;
    std::vector<io_info> m_infos;
};

task<> func(std::vector<int>& vec, int val)
{
    vec.push_back(val);
    co_return;
}

void io_cb(io_info* info, int res)
{
    auto num = reinterpret_cast<int*>(info->data);
    *num     = res;
}

void io_cb_resume(io_info* info, int res)
{
    auto num = reinterpret_cast<int*>(info->data);
    *num     = res;
    local_engine().submit_task(info->handle); // 提交 协程句柄
}

struct test_noop_awaiter
{
    test_noop_awaiter(engine& engine, io_info& info, int* data) noexcept : m_engine(engine), m_info(info)
    {
        // 其次提交 i/o任务
        m_sqe       = m_engine.get_free_urs();
        m_info.data = reinterpret_cast<uintptr_t>(data);
        m_info.cb   = io_cb_resume; // data = res 回调，并提交 协程句柄

        io_uring_prep_nop(m_sqe); // ceq 返回 res=0
        io_uring_sqe_set_data(m_sqe, &m_info);

        m_engine.add_io_submit();
    }

    constexpr auto await_ready() noexcept -> bool { return false; }

    auto await_suspend(std::coroutine_handle<> handle) noexcept -> void
    {
        // 3. 把当前的 句柄保存到 io_info
        m_info.handle = handle;
    }

    constexpr auto await_resume() noexcept -> void {}

    engine&  m_engine;
    io_info& m_info;
    ursptr   m_sqe;
};

// 首先作为 任务存在
task<> noop_io_task(engine& engine, io_info& info, int* data)
{
    // 执行流程
    // 1. engine 执行这个 句柄
    // 2. 创建 test_noop_awaiter
    co_await test_noop_awaiter(engine, info, data);
    // 3. 待外部engine submit后，m_sqe就能提交。
    // 4. 待engine 得到 cqe entry 执行 data.cb(data, res);   //即执行 io_cb_resume
    // 5. 执行 io_cb_resume 把 io_info.data = res = 0 ; 把 句柄作为 任务提交 engine
    // 6. engine 恢复 句柄，重新 调用 test_noop_awaiter::await_resume，回到这里。
    co_return;
}

/*************************************************************
 *                          tests                            *
 *************************************************************/

// test whether the initial state is correct
TEST_F(EngineTest, InitStateCase)
{
    ASSERT_FALSE(m_engine.ready());
    ASSERT_TRUE(m_engine.empty_io());
    ASSERT_EQ(m_engine.num_task_schedule(), 0);
    ASSERT_EQ(m_engine.get_id(), local_engine().get_id());
}

// test submit detach task but exec by user
TEST_F(EngineTest, ExecOneDetachTaskByUser)
{
    auto task = func(m_vec, 1);          // 不立即执行
    m_engine.submit_task(task.handle()); // 提交任务到engine任务队列并唤醒engine，存放的是句柄，并不是 task<>
    task.detach();                       // 放弃所有权，由用户处理

    ASSERT_TRUE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 1);
    auto handle = m_engine.schedule(); // 返回 task
    ASSERT_FALSE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 0);
    handle.resume(); // 恢复 task 运行
    ASSERT_EQ(m_vec.size(), 1);
    ASSERT_EQ(m_vec[0], 1);
    handle.destroy(); // 外部 释放
}

// test submit many detach tasks but exec by user
TEST_F(EngineTest, ExecMultiDetachTaskByUser)
{
    const int task_num = 100;
    for (int i = 0; i < task_num; i++)
    {
        auto task = func(m_vec, i);
        m_engine.submit_task(task.handle());
        task.detach();
    }

    ASSERT_TRUE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), task_num);
    while (m_engine.ready())
    {
        auto handle = m_engine.schedule();
        handle.resume();
        handle.destroy();
    }
    ASSERT_EQ(m_engine.num_task_schedule(), 0);
    ASSERT_EQ(m_vec.size(), task_num);

    std::sort(m_vec.begin(), m_vec.end());
    for (int i = 0; i < task_num; i++)
    {
        ASSERT_EQ(m_vec[i], i);
    }
}

// test submit detach task but exec by engine, the engine
// should destroy task handler, otherwise memory leaks
TEST_F(EngineTest, ExecOneDetachTaskByEngine)
{
    auto task = func(m_vec, 1);
    m_engine.submit_task(task.handle());
    task.detach();

    ASSERT_TRUE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 1);

    m_engine.exec_one_task();

    ASSERT_FALSE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 0);

    ASSERT_EQ(m_vec.size(), 1);
    ASSERT_EQ(m_vec[0], 1);
}

// test submit many detach task but exec by engine, the engine
// should destroy task handler, otherwise memory leaks
TEST_F(EngineTest, ExecMultiDetachTaskByEngine)
{
    const int task_num = 100;
    for (int i = 0; i < task_num; i++)
    {
        auto task   = func(m_vec, i);
        auto handle = task.handle();
        task.detach();
        m_engine.submit_task(handle);
    }

    ASSERT_TRUE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), task_num);
    while (m_engine.ready())
    {
        m_engine.exec_one_task();
    }
    ASSERT_EQ(m_engine.num_task_schedule(), 0);
    ASSERT_EQ(m_vec.size(), task_num);

    std::sort(m_vec.begin(), m_vec.end());
    for (int i = 0; i < task_num; i++)
    {
        ASSERT_EQ(m_vec[i], i);
    }
}

// test submit task but exec by engine, the engine
// shouldn't destroy task handler, otherwise cause pointer double free.
TEST_F(EngineTest, ExecOneNoDetachTaskByEngine)
{
    auto task = func(m_vec, 1);
    m_engine.submit_task(task.handle());

    ASSERT_TRUE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 1);
    m_engine.exec_one_task();
    ASSERT_FALSE(m_engine.ready());
    ASSERT_EQ(m_engine.num_task_schedule(), 0);

    ASSERT_EQ(m_vec.size(), 1);
    ASSERT_EQ(m_vec[0], 1);
}

// TODO: Add more nopio tests for engine
// // test add nop-io before engine poll
// TEST_F(EngineTest, AddNopIOBeforePoll)
// {
//     io_info info;
//     m_vec.push_back(1);

//     auto io_thread = std::thread(
//         [&]()
//         {
//             info.data = reinterpret_cast<uintptr_t>(&m_vec[0]);
//             info.cb   = io_cb;
//             auto sqe  = m_engine.get_free_urs();
//             ASSERT_NE(sqe, nullptr);
//             io_uring_prep_nop(sqe);
//             io_uring_sqe_set_data(sqe, &info);
//             m_engine.add_io_submit();
//         });

//     auto poll_thread = std::thread(
//         [&]()
//         {
//             utils::msleep(100); // ensure io_thread finish first
//             do
//             {
//                 m_engine.poll_submit();
//             } while (!m_engine.empty_io());
//         });

//     io_thread.join();
//     poll_thread.join();

//     ASSERT_EQ(m_vec[0], 0);
// }

// // test add nop-io after engine poll
// TEST_F(EngineTest, AddNopIOAfterPoll)
// {
//     io_info info;
//     m_vec.push_back(1);

//     auto io_thread = std::thread(
//         [&]()
//         {
//             utils::msleep(100);
//             info.data = reinterpret_cast<uintptr_t>(&m_vec[0]);
//             info.cb   = io_cb;
//             auto sqe  = m_engine.get_free_urs();
//             ASSERT_NE(sqe, nullptr);
//             io_uring_prep_nop(sqe);
//             io_uring_sqe_set_data(sqe, &info);
//             m_engine.add_io_submit();
//         });

//     auto poll_thread = std::thread(
//         [&]()
//         {
//             do
//             {
//                 m_engine.poll_submit();
//             } while (!m_engine.empty_io());
//         });

//     io_thread.join();
//     poll_thread.join();

//     ASSERT_EQ(m_vec[0], 0);
// }

// test add batch nop-io
TEST_P(EngineNopIOTest, AddBatchNopIO)
{
    int task_num = GetParam();
    m_infos.resize(task_num);
    m_vec = std::vector<int>(task_num, 1);
    for (int i = 0; i < task_num; i++)
    {
        m_infos[i].data = reinterpret_cast<uintptr_t>(&m_vec[i]);
        m_infos[i].cb   = io_cb;
        auto sqe        = m_engine.get_free_urs(); // 获取sqe entry
        ASSERT_NE(sqe, nullptr);
        // io_uring_prep_nop(sqe)  准备提交的是
        // ​空操作请求​（IORING_OP_NOP）。该操作不执行任何实际
        // I/O（如读写文件或网络），仅由内核生成一个完成事件（CQE）返回用户态
        io_uring_prep_nop(sqe);
        io_uring_sqe_set_data(sqe, &m_infos[i]);
        m_engine.add_io_submit(); // 待提交任务数+1
    }

    do
    {
        m_engine.poll_submit(); // engine 主要函数，提交io，等待io，获取io结果，处理io结果
    } while (!m_engine.empty_io()); // 只要 engine 不为空（待提交/待完成 任务还有）那么就 engine 阻塞 eventfd等待。

    for (int i = 0; i < task_num; i++)
    {
        ASSERT_EQ(m_vec[i], 0);
    }
}

INSTANTIATE_TEST_SUITE_P(EngineNopIOTests, EngineNopIOTest, ::testing::Values(1, 100, 10000));

// test add nop-io in loop
TEST_F(EngineTest, LoopAddNopIO)
{
    const int loop_num = 2 * config::kEntryLength;
    m_vec.push_back(0);
    for (int i = 0; i < loop_num; i++)
    {
        m_vec[0] = 1;
        io_info info;
        info.data = reinterpret_cast<uintptr_t>(&m_vec[0]);
        info.cb   = io_cb;

        auto sqe = m_engine.get_free_urs();
        ASSERT_NE(sqe, nullptr);
        io_uring_prep_nop(sqe);
        io_uring_sqe_set_data(sqe, &info);
        m_engine.add_io_submit();

        do
        {
            m_engine.poll_submit();
        } while (!m_engine.empty_io());
        ASSERT_EQ(m_vec[0], 0);
    }
}

// test submit task after engine poll
TEST_F(EngineTest, LastSubmitTaskToEngine)
{
    m_vec.push_back(0);
    auto task = func(m_vec, 0);

    auto t1 = std::thread(
        [&]()
        {
            m_engine.poll_submit(); // 没有任务，就 阻塞 eventfd
            m_vec[0] = 1;
            ASSERT_TRUE(m_engine.ready());
            ASSERT_EQ(m_engine.num_task_schedule(), 1);
            ASSERT_TRUE(m_engine.empty_io());
        });
    auto t2 = std::thread(
        [&]()
        {
            base::msleep(100);
            m_vec[0] = 2;
            m_engine.submit_task(task.handle()); // 提交任务，唤醒 engine
        });
    t1.join();
    t2.join();

    ASSERT_EQ(m_vec[0], 1);
}

// test submit task before engine poll
TEST_F(EngineTest, FirstSubmitTaskToEngine)
{
    m_vec.push_back(0);
    auto task = func(m_vec, 0);

    auto t1 = std::thread(
        [&]()
        {
            base::msleep(100);
            m_engine.poll_submit(); // 没有 io 任务，就 阻塞 eventfd
            m_vec[0] = 1;           // 唤醒后，但没有执行任务，所以是1不是0
            ASSERT_TRUE(m_engine.ready());
            ASSERT_EQ(m_engine.num_task_schedule(), 1);
            ASSERT_TRUE(m_engine.empty_io());
        });
    auto t2 = std::thread(
        [&]()
        {
            m_vec[0] = 2;
            m_engine.submit_task(task.handle()); // 先提交任务
        });
    t1.join();
    t2.join();

    ASSERT_EQ(m_vec[0], 1);
}

// test submit task but exec by user
TEST_F(EngineTest, SubmitTaskToEngineExecByUser)
{
    m_vec.push_back(0);
    auto task = func(m_vec, 2);

    auto t1 = std::thread(
        [&]()
        {
            m_engine.poll_submit();
            m_vec[0] = 1;
            ASSERT_TRUE(m_engine.ready());
            ASSERT_EQ(m_engine.num_task_schedule(), 1);
            ASSERT_TRUE(m_engine.empty_io());
        });
    auto t2 = std::thread(
        [&]()
        {
            base::msleep(100);
            m_vec[0] = 2;
            m_engine.submit_task(task.handle());
        });
    t1.join();
    t2.join();

    task.handle().resume();

    ASSERT_EQ(m_vec.size(), 2);
    ASSERT_EQ(m_vec[0], 1);
    ASSERT_EQ(m_vec[1], 2);
}

// test submit task but exec by engine
TEST_F(EngineTest, SubmitTaskToEngineExecByEngine)
{
    m_vec.push_back(0);
    auto task = func(m_vec, 2);

    auto t1 = std::thread(
        [&]()
        {
            m_engine.poll_submit(); // 1
            m_vec[0] = 1;           // 4 m_vec[1] = 1
            ASSERT_TRUE(m_engine.ready());
            ASSERT_EQ(m_engine.num_task_schedule(), 1);
            ASSERT_TRUE(m_engine.empty_io());
            m_engine.exec_one_task(); // 5 执行了句柄，m_vec[1] = 2
        });
    auto t2 = std::thread(
        [&]()
        {
            msleep(100);
            m_vec[0] = 2;                        // 2
            m_engine.submit_task(task.handle()); // 3
        });
    t1.join();
    t2.join();

    ASSERT_EQ(m_vec.size(), 2);
    ASSERT_EQ(m_vec[0], 1);
    ASSERT_EQ(m_vec[1], 2);
}

// test submit task by multi-thread
TEST_P(EngineMultiThreadTaskTest, MultiThreadAddTask)
{
    int thread_num = GetParam();

    auto t = std::thread(
        [&]()
        {
            int count = 0;
            while (count < thread_num)
            {
                m_engine.poll_submit();
                while (m_engine.ready())
                {
                    ++count;
                    m_engine.exec_one_task();
                }
            }
        });

    std::vector<std::thread> vec;
    for (int i = 0; i < thread_num; i++)
    {
        vec.push_back(std::thread(
            [&, i]()
            {
                auto task   = func(m_vec, i);
                auto handle = task.handle();
                task.detach();
                m_engine.submit_task(handle);
            }));
    }

    t.join();
    for (int i = 0; i < thread_num; i++)
    {
        vec[i].join();
    }

    std::sort(m_vec.begin(), m_vec.end());
    for (int i = 0; i < thread_num; i++)
    {
        EXPECT_EQ(m_vec[i], i);
    }
}

INSTANTIATE_TEST_SUITE_P(EngineMultiThreadTaskTests, EngineMultiThreadTaskTest, ::testing::Values(1, 10, 100));

// test submit task and nop-io
TEST_P(EngineMixTaskNopIOTest, MixTaskNopIO)
{
    int task_num, nopio_num;
    std::tie(task_num, nopio_num) = GetParam();
    m_vec.reserve(task_num + nopio_num + 1);
    m_vec.resize(nopio_num);
    m_infos.resize(nopio_num);

    auto task_thread = std::thread(
        [&]()
        {
            for (int i = 0; i < task_num; i++)
            {
                auto task   = func(m_vec, nopio_num + i); // puhs_back
                auto handle = task.handle();
                task.detach();
                m_engine.submit_task(handle);
                if ((i + 1) % 100 == 0)
                {
                    msleep(10);
                }
            }
        });

    auto io_thread = std::thread(
        [&]()
        {
            for (int i = 0; i < nopio_num; i++)
            {
                // m_infos[i].data = reinterpret_cast<uintptr_t>(&m_vec[i]);
                // m_infos[i].cb   = io_cb;

                // auto sqe = m_engine.get_free_urs();
                // ASSERT_NE(sqe, nullptr);
                // io_uring_prep_nop(sqe);
                // io_uring_sqe_set_data(sqe, &m_infos[i]);

                // m_engine.add_io_submit();
                auto task = noop_io_task(m_engine, m_infos[i], &(m_vec[i]));
                // 分析后，最终 data m_vec[i] 会赋值为 res=0
                auto handle = task.handle();
                task.detach();
                m_engine.submit_task(handle);
                if ((i + 1) % 100 == 0)
                {
                    msleep(10);
                }
            }

            // these will make poll_thread finish after io_thread
            auto task   = func(m_vec, task_num + nopio_num);
            auto handle = task.handle();
            task.detach();
            m_engine.submit_task(handle);
        });

    auto poll_thread = std::thread(
        [&]()
        {
            linfo.egn = &m_engine; // Must set this

            int cnt = 0;
            // only all task been processed, poll thread will finish
            // 提交一次noop_io_task任务，内部awaitable还会提交一次i/o任务，所以是2 * nopio_num
            while (cnt < 2 * nopio_num + task_num + 1)
            {
                m_engine.poll_submit(); // 提交
                while (m_engine.ready())
                {
                    m_engine.exec_one_task();
                    cnt++;
                }
            }
        });

    task_thread.join();
    io_thread.join();
    poll_thread.join();

    ASSERT_EQ(m_vec.size(), task_num + nopio_num + 1);
    std::sort(m_vec.begin(), m_vec.end());
    for (int i = 0; i < nopio_num; i++)
    {
        ASSERT_EQ(m_vec[i], 0);
    }
    for (int i = 0; i < task_num + 1; i++)
    {
        ASSERT_EQ(m_vec[i + nopio_num], i + nopio_num);
    }
}

INSTANTIATE_TEST_SUITE_P(
    EngineMixTaskNopIOTests,
    EngineMixTaskNopIOTest,
    ::testing::Values(
        std::make_tuple(1, 1), std::make_tuple(100, 100), std::make_tuple(1000, 1000), std::make_tuple(10000, 10000)));
