#include "network/net/eventloop.h"
#include "base/log/log.h"
#include "base/macro.h"
#include "base/utils/utils.h"
#include "network/net/event.h"
#include <cerrno>
#include <cstring>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tmms::net
{
static thread_local EventLoop* t_local_eventloop = nullptr;

EventLoop::EventLoop() : epoll_fd_(::epoll_create(1024)), epoll_events_(1024)
{
    if (t_local_eventloop)
    {
        return;
    }
    t_local_eventloop = this;
}
EventLoop::~EventLoop()
{
    Quit();
}
void EventLoop::loop()
{
    looping_        = true;
    int64_t timeout = 1000;
    while (looping_)
    {
        ::memset(&epoll_events_[0], 0x00, sizeof(struct epoll_event) * epoll_events_.size());
        auto ret = ::epoll_wait(
            epoll_fd_, (struct epoll_event*)&epoll_events_[0], static_cast<int>(epoll_events_.size()), timeout);

        if (ret >= 0)
        {
            for (int i = 0; i < ret; ++i)
            {
                struct epoll_event& ev = epoll_events_[i];
                if (ev.data.fd <= 0)
                {
                    continue;
                }

                auto iter = events_.find(ev.data.fd);
                if (iter == events_.end())
                {
                    continue;
                }
                std::shared_ptr<Event>& event = iter->second;

                if (ev.events & EPOLLERR) // 出错事件
                {
                    int       error = 0;
                    socklen_t len   = sizeof(error);
                    // 读错误
                    getsockopt(event->Fd(), SOL_SOCKET, SO_ERROR, &error, &len);

                    event->OnError(strerror(error)); // 处理子类处理
                }
                else if ((ev.events & EPOLLHUP) && !(ev.events & EPOLLIN)) // 挂起（关闭）事件
                {
                    event->OnClose();
                }
                else if (ev.events & (EPOLLIN | EPOLLPRI))
                // 读标志 和tcp带外数据（制定了紧急数据的）的，send加了MSG_OOB标志的数据
                {
                    event->OnRead();
                }
                else if (ev.events & EPOLLOUT)
                {
                    event->OnWrite();
                }
            }

            if (ret == epoll_events_.size()) // 事件吃满了数组，说明并发量很高，需要扩充事件数组
            {
                epoll_events_.resize(epoll_events_.size() * 2);
            }
            // 处理任务队列
            RunFunctions();
            // 任务队列里会有添加定时任务的 任务 ~

            // 处理定时任务
            timingWheel_.OnTimer(base::TTime::NowMS());
        }
        else if (ret < 0)
        { // error
            CORE_ERROR("epoll wait error.error:{}", errno);
        }
    }
}
void EventLoop::Quit()
{
    looping_ = false;
}

void EventLoop::AddEvent(const std::shared_ptr<Event>& event)
{
    // 因为这个事件只在一个线程loop内执行，不用加锁争用
    auto iter = events_.find(event->Fd());
    if (iter != events_.end())
    {
        CORE_ERROR("find event fd : {}", event->Fd());
        return;
    }
    event->event_ |= kEventRead;
    events_[event->Fd()] = event;

    struct epoll_event ev;
    memset(&ev, 0x00, sizeof(struct epoll_event));

    ev.events  = event->event_;
    ev.data.fd = event->fd_;

    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event->fd_, &ev))
    {
        CORE_ERROR("epoll_ctl_add failed! errno = {}", errno);
    }
}

void EventLoop::DelEvent(const std::shared_ptr<Event>& event)
{
    // 因为这个事件只在一个线程loop内执行，不用加锁争用
    auto iter = events_.find(event->Fd());
    if (iter == events_.end())
    {
        CORE_ERROR("cant find event fd : {}", event->Fd());
        return;
    }

    events_.erase(iter);

    struct epoll_event ev;
    memset(&ev, 0x00, sizeof(struct epoll_event));

    ev.events  = event->event_;
    ev.data.fd = event->fd_;

    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event->fd_, &ev))
    {
        CORE_ERROR("epoll_ctl_add failed! errno = {}", errno);
    }
}

/// @brief 使fd能写，就是添加EPOLLOUT标志
/// @param event  为true，就开启写，为false，原来有写的也会被取消
/// @param enable
/// @return 使能成功
bool EventLoop::EnableEventWriting(const std::shared_ptr<Event>& event, bool enable)
{
    auto iter = events_.find(event->Fd());
    if (iter == events_.end())
    {
        CORE_ERROR("cant find event fd : {}", event->Fd());
        return false;
    }

    if (enable)
    {
        event->event_ |= kEventWrite;
    }
    else
    {
        event->event_ &= ~kEventWrite;
    }

    struct epoll_event ev;
    memset(&ev, 0x00, sizeof(struct epoll_event));

    ev.events  = event->event_;
    ev.data.fd = event->fd_;
    // 添加epoll事件
    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, event->fd_, &ev))
    {
        CORE_ERROR("epoll_ctl_mod failed! errno = {}", errno);
        return false;
    }
    return true;
}
bool EventLoop::EnableEventReading(const std::shared_ptr<Event>& event, bool enable)
{
    auto iter = events_.find(event->Fd());
    if (iter == events_.end())
    {
        CORE_ERROR("cant find event fd : {}", event->Fd());
        return false;
    }

    if (enable)
    {
        event->event_ |= kEventRead;
    }
    else
    {
        event->event_ &= ~kEventRead;
    }

    struct epoll_event ev;
    memset(&ev, 0x00, sizeof(struct epoll_event));

    ev.events  = event->event_;
    ev.data.fd = event->fd_;
    // 添加epoll事件
    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, event->fd_, &ev))
    {
        CORE_ERROR("epoll_ctl_mod failed! errno = {}", errno);
        return false;
    }
    return true;
}

// 任务队列相关
void EventLoop::AssertInLoopThread() // 是否在同一个循环
{
    CORE_ASSERT2(!IsInLoopThread(), "It is forbidden to run loop on other thread!!!");
}
bool EventLoop::IsInLoopThread() const
{
    return t_local_eventloop == this;
}

/// @brief 传入任务，如果调用任务的线程和loop是在同一个线程内的，就直接执行，如果不是就加入任务队列
/// 目的就是：为了保证一个任务是在同一个线程中执行的，而不会因为不在同一个线程中执行，执行到一半
/// 就被其他线程抢占执行，错乱
/// 如果是在同一个线程，那么执行就可以了，可以保证执行都是在一个线程内
/// 如果不是在同一个线程，加入任务队列，loop执行，因为loop是在一个线程执行，所以执行的任务也是在一个线程内执行
/// @param f  执行任务的函数
void EventLoop::RunInLoop(const Func& f)
{
    if (IsInLoopThread())
    {
        // 如果某个任务（通过 RunInLoop 提交）在执行过程中又调用了 RunInLoop 提交新任务，由于任务在 EventLoop
        // 线程执行，嵌套调用时 IsInLoopThread() 也会返回 true，新任务会直接执行。
        f();
    }
    else
    {
        std::lock_guard<std::mutex> lk(lock_);
        functions_.push(f);

        WakeUp();
    }
}
void EventLoop::RunInLoop(const Func&& f)
{
    if (IsInLoopThread())
    {
        f();
    }
    else
    {
        std::lock_guard<std::mutex> lk(lock_);
        functions_.push(std::move(f));

        WakeUp();
    }
}

/// @brief 执行任务队列里的任务
void EventLoop::RunFunctions()
{
    std::lock_guard<std::mutex> lk(lock_);
    while (!functions_.empty())
    {
        auto& f = functions_.front();
        f();
        functions_.pop();

        /**
         * bug经历：
         *
        functions_.pop();
        f();
        这样写出问题了，因为f是引用的，先删除再引用函数就出错了
         *
        */
    }
}

void EventLoop::WakeUp()
{
    if (!pipe_event_)
    {
        pipe_event_ = std::make_shared<PipeEvent>(this);
        AddEvent(pipe_event_);
    }

    int64_t tmp = 1;
    pipe_event_->Write((const char*)&tmp, sizeof(tmp));

} // 唤醒loop

// 时间轮
void EventLoop::InstertEntry(uint32_t delay, EntryPtr entryPtr) // 插入entry，设置超时时间
{
    if (IsInLoopThread())
    {
        timingWheel_.InstertEntry(delay, entryPtr);
    }
    else
    {
        RunInLoop([this, delay, entryPtr]() { timingWheel_.InstertEntry(delay, entryPtr); });
    }
}
void EventLoop::AddTimer(double s, const Func& cb, bool recurring)
{
    if (IsInLoopThread())
    {
        timingWheel_.AddTimer(s, cb, recurring);
    }
    else
    {
        RunInLoop([this, s, cb, recurring]() { timingWheel_.AddTimer(s, cb, recurring); });
    }
}
void EventLoop::AddTimer(double s, Func&& cb, bool recurring)
{
    if (IsInLoopThread())
    {
        timingWheel_.AddTimer(s, cb, recurring);
    }
    else
    {
        RunInLoop([this, s, cb, recurring]() { timingWheel_.AddTimer(s, cb, recurring); });
    }
}
void EventLoop::AddConditionTimer(double s, const Func& cb, std::weak_ptr<void> weak_cond, bool recurring)
{
    if (IsInLoopThread())
    {
        timingWheel_.AddConditionTimer(s, cb, weak_cond, recurring);
    }
    else
    {
        RunInLoop([this, s, cb, weak_cond, recurring]()
                  { timingWheel_.AddConditionTimer(s, cb, weak_cond, recurring); });
    }
}
void EventLoop::AddConditionTimer(double s, Func&& cb, std::weak_ptr<void> weak_cond, bool recurring)
{
    if (IsInLoopThread())
    {
        timingWheel_.AddConditionTimer(s, cb, weak_cond, recurring);
    }
    else
    {
        RunInLoop([this, s, cb, weak_cond, recurring]()
                  { timingWheel_.AddConditionTimer(s, cb, weak_cond, recurring); });
    }
}

} // namespace tmms::net