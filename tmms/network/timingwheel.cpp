#include "network/timingwheel.h"
#include "base/log/log.h"
#include <memory>

namespace tmms::net
{
TimingWheel::TimingWheel() : wheels_(4)
{
    wheels_[kTimingTypeSecond].resize(60);
    wheels_[kTimingTypeMinute].resize(60);
    wheels_[kTimingTypeHour].resize(24);
    wheels_[kTimingTypeDay].resize(30);
}
TimingWheel::~TimingWheel()
{
}
/// @brief  判断插入的时间，循环调用的方式最后执行任务插入到秒的时间轮
/// @param delay 单位秒
/// @param entryPtr
void TimingWheel::InstertEntry(uint32_t delay, EntryPtr entryPtr) // 插入entry，设置超时时间
{
    if (delay <= 0)
    {
        entryPtr.reset();
    }
    else if (delay < KTimingMinute)
    {
        InstertSecondEntry(delay, entryPtr);
    }
    else if (delay < KTimingHour)
    {
        InstertMinuteEntry(delay, entryPtr);
    }
    else if (delay < KTimingDay)
    {
        InstertHourEntry(delay, entryPtr);
    }
    else
    {
        auto day = delay / KTimingDay;
        if (day > 30)
        {
            CORE_ERROR("It is not surpport > 30 day!!!");
            return;
        }
        InstertDayEntry(delay, entryPtr);
    }
}

/// @brief  转动时间轮，记录转动的秒数判断是否执行分，时，天的时间轮
/// @param now 单位毫秒
void TimingWheel::OnTimer(int64_t now)
{

    if (last_ts_ == 0)
    {
        last_ts_ = now;
    }
    if (now - last_ts_ < 1000)
    { // 1s时间执行一次秒的时间轮
        return;
    }
    last_ts_ = now;
    ++tick_;
    PopUp(wheels_[kTimingTypeSecond]);

    if (tick_ % KTimingMinute == 0) // 转动了60s的倍数，也就是分钟，处理分钟
    {
        PopUp(wheels_[kTimingTypeMinute]);
    }
    else if (tick_ % KTimingHour == 0)
    {
        PopUp(wheels_[kTimingTypeHour]);
    }
    else if (tick_ % KTimingDay == 0)
    {
        PopUp(wheels_[kTimingTypeDay]);
    }
}

void TimingWheel::PopUp(Wheel& bq)
{
    WheelEntry tmp;
    // bq.front().clear(); // bug: clear()清理元素但是不调用析构
    bq.front().swap(tmp);       // 这样会调用析构，总是deque的头部先到期的，清理头部，头部是unordered_set
    bq.pop_front();             // 弹出前面的set
    bq.push_back(WheelEntry()); // 插一个新的槽，保证槽的数量不变
}

void TimingWheel::AddTimer(double s, const Func& cb, bool recurring)
{
    CallbackEntry::ptr cbEntry = std::make_shared<CallbackEntry>(
        [this, s, cb, recurring]()
        {
            cb();
            if (recurring)
            {
                AddTimer(s, cb, true);
            }
        });
    InstertEntry(s, cbEntry);
}

void TimingWheel::AddTimer(double s, Func&& cb, bool recurring)
{
    CallbackEntry::ptr cbEntry = std::make_shared<CallbackEntry>(
        [this, s, cb = std::move(cb), recurring]()
        {
            cb();
            if (recurring)
            {
                AddTimer(s, cb, true);
            }
        });
    InstertEntry(s, cbEntry);
}

void TimingWheel::AddConditionTimer(double s, const Func& cb, std::weak_ptr<void> weak_cond, bool recurring)
{
    CallbackEntry::ptr cbEntry = std::make_shared<CallbackEntry>(
        [this, s, cb, weak_cond, recurring]()
        {
            auto cond = weak_cond.lock();
            if (cond)
            {
                cb();
                if (recurring)
                {
                    AddConditionTimer(s, cb, weak_cond, recurring);
                }
            }
        });
    InstertEntry(s, cbEntry);
}

void TimingWheel::AddConditionTimer(double s, Func&& cb, std::weak_ptr<void> weak_cond, bool recurring)
{
    CallbackEntry::ptr cbEntry = std::make_shared<CallbackEntry>(
        [this, s, cb, weak_cond, recurring]()
        {
            auto cond = weak_cond.lock();
            if (cond)
            {
                cb();
                if (recurring)
                {
                    AddConditionTimer(s, cb, weak_cond, recurring);
                }
            }
        });
    InstertEntry(s, cbEntry);
}

void TimingWheel::InstertSecondEntry(uint32_t delay, EntryPtr entryPtr)
{
    wheels_[kTimingTypeSecond][delay - 1].emplace(entryPtr);
}

void TimingWheel::InstertMinuteEntry(uint32_t delay, EntryPtr entryPtr)
{
    auto minute = delay / KTimingMinute;
    auto second = delay % KTimingMinute;

    CallbackEntry::ptr newEntryPtr =
        std::make_shared<CallbackEntry>([this, second, entryPtr]() { InstertEntry(second, entryPtr); });

    wheels_[kTimingTypeMinute][minute - 1].emplace(newEntryPtr);
}

void TimingWheel::InstertHourEntry(uint32_t delay, EntryPtr entryPtr)
{
    auto hour   = delay / KTimingHour;
    auto second = delay % KTimingHour;

    CallbackEntry::ptr newEntryPtr =
        std::make_shared<CallbackEntry>([this, second, entryPtr]() { InstertEntry(second, entryPtr); });
    wheels_[kTimingTypeHour][hour - 1].emplace(newEntryPtr);
}

void TimingWheel::InstertDayEntry(uint32_t delay, EntryPtr entryPtr)
{
    auto day    = delay / KTimingDay;
    auto second = delay % KTimingDay;

    CallbackEntry::ptr newEntryPtr =
        std::make_shared<CallbackEntry>([this, second, entryPtr]() { InstertEntry(second, entryPtr); });
    wheels_[kTimingTypeDay][day - 1].emplace(newEntryPtr);
}

} // namespace tmms::net