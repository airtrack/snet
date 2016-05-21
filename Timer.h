#ifndef TIMER_H
#define TIMER_H

#include "EventLoop.h"
#include <chrono>
#include <functional>
#include <set>

namespace snet
{

using TimePoint = std::chrono::steady_clock::time_point;
using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;

class TimerList;

class Timer final
{
public:
    class Handle final
    {
    public:
        explicit Handle(Timer *timer)
            : timer_(timer)
        {
        }

        Handle(const Handle &) = delete;
        void operator = (const Handle &) = delete;

        TimePoint GetTimePoint() const
        {
            return timer_->time_point_;
        }

        void Timeout()
        {
            if (timer_->on_timeout_)
                timer_->on_timeout_();
        }

    private:
        Timer *timer_;
    };

    using OnTimeout = std::function<void ()>;

    explicit Timer(TimerList *timer_list);

    Timer(const Timer &) = delete;
    void operator = (const Timer &) = delete;

    template<typename DurationType>
    void ExpireFromNow(const DurationType &duration)
    {
        ExpireAt(std::chrono::steady_clock::now() + duration);
    }

    void ExpireAt(const TimePoint &time_point);
    void SetOnTimeout(const OnTimeout &on_timeout);
    void Cancel();

private:
    TimerList *timer_list_;
    TimePoint time_point_;
    OnTimeout on_timeout_;
    Handle handle_;
};

class TimerList final
{
public:
    TimerList();

    TimerList(const TimerList &) = delete;
    void operator = (const TimerList &) = delete;

    void AddTimer(Timer::Handle *timer);
    void DelTimer(Timer::Handle *timer);
    void TickTock();

private:
    using TimerSet = std::set<std::pair<TimePoint, Timer::Handle *>>;

    TimerSet timer_set_;
};

class TimerDriver final : public LoopHandler
{
public:
    explicit TimerDriver(TimerList &timer_list);

    TimerDriver(const TimerDriver &) = delete;
    void operator = (const TimerDriver &) = delete;

    virtual void HandleLoop() override;
    virtual void HandleStop() override { }

private:
    TimerList &timer_list_;
};

} // namespace snet

#endif // TIMER_H
