/*
 * @Author: star-cs
 * @Date: 2025-07-21 16:00:25
 * @LastEditTime: 2025-07-21 22:31:27
 * @FilePath: /TMMS-SERVER/tmms/network/eventloop.h
 * @Description:
 */
#pragma once

#include "network/event.h"
#include <functional>
#include <memory>
#include <queue>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>

namespace tmms::net
{
using Func = std::function<void()>;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();
    void loop(); // 事件循环中
    void Quit();
    // 下面的操作对外调用 由 第三方线程
    void AddEvent(const std::shared_ptr<Event>& event);
    void DelEvent(const std::shared_ptr<Event>& event);
    bool EnableEventWriting(const std::shared_ptr<Event>& event, bool enable);
    bool EnableEventReading(const std::shared_ptr<Event>& event, bool enable);

    // 任务队列相关
    void AssertInLoopThread(); // 是否在同一个循环
    bool IsInLoopThread() const;
    void RunInLoop(const Func& f);
    void RunInLoop(const Func&& f);

private:
    void RunFunctions();
    void WakeUp(); // 唤醒loop

    bool                                            looping_{false}; // 是否正在循环
    int                                             epoll_fd_{-1};
    std::vector<struct epoll_event>                 epoll_events_; // epoll事件数组
    std::unordered_map<int, std::shared_ptr<Event>> events_;       // fd和event关联
    std::queue<Func>                                functions_;
    std::mutex                                      lock_;
    std::shared_ptr<PipeEvent>                      pipe_event_;
};

} // namespace tmms::net