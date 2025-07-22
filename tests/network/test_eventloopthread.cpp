/*
 * @Author: star-cs
 * @Date: 2025-07-21 20:19:44
 * @LastEditTime: 2025-07-22 20:53:27
 * @FilePath: /TMMS-SERVER/tests/network/test_eventloopthread.cpp
 * @Description:
 */
#include "base/log/log.h"
#include "base/utils/utils.h"
#include "network/event.h"
#include "network/eventloop.h"
#include "network/eventloopthread.h"
#include "network/eventloopthreadpool.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>

using namespace tmms::net;
using namespace tmms::base;

EventLoopThread eventloop_thread; // 无参初始化，生成一个线程
std::thread     th;

void test()
{
    CORE_DEBUG("test start");
    eventloop_thread.Run(); // 启动事件循环
    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        std::cout << "loop : " << loop << std::endl;
        std::shared_ptr<PipeEvent> pipe = std::make_shared<PipeEvent>(loop);
        loop->AddEvent(pipe);
        int64_t test = 12345;
        pipe->Write((const char*)&test, sizeof(test));
        th = std::thread(
            [&pipe]()
            {
                while (1)
                {
                    auto    currentTime = std::chrono::system_clock::now().time_since_epoch();
                    int64_t timestamp   = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
                    pipe->Write((const char*)&timestamp, sizeof(timestamp));
                }
            });
        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void test2()
{
    std::cout << " main thread id = " << std::this_thread::get_id() << std::endl;
    EventLoopThreadPool pool(2, 0, 2);
    pool.Start();

    // std::vector<EventLoop*> lists = pool.GetLoops();
    // for (auto& eventloop : lists)
    // {
    //     // 这里还是主线程
    //     eventloop->RunInLoop(
    //         [&eventloop]()
    //         { std::cout << "loop : " << eventloop << "   this id = " << std::this_thread::get_id() << std::endl; });
    // }

    EventLoop* loop = pool.GetNextLoop();
    std::cout << "loop : " << loop << std::endl;
    loop->AddTimer(1, []() { CORE_DEBUG("1 second"); });
    loop->AddTimer(5, []() { CORE_DEBUG("5 second"); });

    loop->AddTimer(1, []() { CORE_DEBUG("every 1 second"); }, true);
    loop->AddTimer(5, []() { CORE_DEBUG("every 5 second"); }, true);

    // loop = pool.GetNextLoop();
    // std::cout << "loop : " << loop << std::endl;
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    Log::init(true);

    // test();
    test2();

    return 0;
}