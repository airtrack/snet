#include "Timer.h"
#include <vector>

namespace snet
{

Timer::Timer(TimerList *timer_list)
    : timer_list_(timer_list),
      handle_(this)
{
}

void Timer::ExpireAt(const TimePoint &time_point)
{
    timer_list_->DelTimer(&handle_);
    time_point_ = time_point;
    timer_list_->AddTimer(&handle_);
}

void Timer::SetOnTimeout(const OnTimeout &on_timeout)
{
    on_timeout_ = on_timeout;
}

void Timer::Cancel()
{
    timer_list_->DelTimer(&handle_);
}

TimerList::TimerList()
{
}

void TimerList::AddTimer(Timer::Handle *timer)
{
    timer_set_.insert(
        std::make_pair(timer->GetTimePoint(), timer));
}

void TimerList::DelTimer(Timer::Handle *timer)
{
    timer_set_.erase(
        std::make_pair(timer->GetTimePoint(), timer));
}

void TimerList::TickTock()
{
    auto now = std::chrono::steady_clock::now();
    auto max_ptr = reinterpret_cast<Timer::Handle *>(~uintptr_t(0));

    auto begin = timer_set_.begin();
    auto end = timer_set_.lower_bound(std::make_pair(now, max_ptr));

    if (begin != end)
    {
        std::vector<TimerSet::value_type> expired(begin, end);
        timer_set_.erase(begin, end);

        for (auto &pair : expired)
            pair.second->Timeout();
    }
}

TimerDriver::TimerDriver(TimerList &timer_list)
    : timer_list_(timer_list)
{
}

void TimerDriver::HandleLoop()
{
    timer_list_.TickTock();
}

} // namespace snet
