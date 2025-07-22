#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <unordered_set>

namespace tmms::net
{
using EntryPtr   = std::shared_ptr<void>;
using WheelEntry = std::unordered_set<EntryPtr>;
using Wheel      = std::deque<WheelEntry>;
using Wheels     = std::vector<Wheel>;
using Func       = std::function<void()>;

constexpr int KTimingMinute = 60;
constexpr int KTimingHour   = 60 * 60;
constexpr int KTimingDay    = 60 * 60 * 24;

enum TimingType
{
    kTimingTypeSecond = 0,
    kTimingTypeMinute = 1,
    kTimingTypeHour   = 2,
    kTimingTypeDay    = 3,
};

class CallbackEntry
{
public:
    using ptr = std::shared_ptr<CallbackEntry>;
    CallbackEntry(const Func& cb) : cb_(cb) {}
    ~CallbackEntry()
    {
        if (cb_) // 从时间轮删除最后一个引用，析构进行回调
        {
            cb_();
        }
    }

private:
    Func cb_;
};

class TimingWheel
{
public:
    using ptr = std::shared_ptr<TimingWheel>;

    TimingWheel();
    ~TimingWheel();

    void InstertEntry(uint32_t delay, EntryPtr entryPtr); // 插入entry，设置超时时间
    void OnTimer(int64_t now);
    void PopUp(Wheel& bq);
    void AddTimer(double s, const Func& cb, bool recurring = false);
    void AddTimer(double s, Func&& cb, bool recurring = false);
    void AddConditionTimer(double s, const Func& cb, std::weak_ptr<void> weak_cond, bool recurring = false);
    void AddConditionTimer(double s, Func&& cb, std::weak_ptr<void> weak_cond, bool recurring = false);

private:
    void InstertSecondEntry(uint32_t delay, EntryPtr entryPtr);
    void InstertMinuteEntry(uint32_t delay, EntryPtr entryPtr);
    void InstertHourEntry(uint32_t delay, EntryPtr entryPtr);
    void InstertDayEntry(uint32_t delay, EntryPtr entryPtr);

    Wheels   wheels_;
    int64_t  last_ts_{0};
    uint64_t tick_{0};
};
} // namespace tmms::net