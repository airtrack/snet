#include "Timer.h"
#include "EventLoop.h"
#include <iostream>

int main()
{
    auto event_loop = snet::CreateEventLoop();

    snet::TimerList timer_list;
    snet::TimerDriver timer_driver(timer_list);

    snet::Timer cancelled_timer(&timer_list);
    cancelled_timer.ExpireFromNow(snet::Minutes(1));
    cancelled_timer.SetOnTimeout(
        [] () { std::cout << "should not be called" << std::endl; });

    snet::Timer timer1(&timer_list);
    timer1.ExpireFromNow(snet::Milliseconds(500));
    timer1.SetOnTimeout(
        [] () { std::cout << "timer1 timeout" << std::endl; });

    auto timer2_times = 1;
    snet::Timer timer2(&timer_list);
    timer2.ExpireFromNow(snet::Seconds(1));
    timer2.SetOnTimeout(
        [&] () {
            std::cout << "timer2 timeout at " << timer2_times
                      << " times" << std::endl;
            ++timer2_times;

            if (timer2_times <= 3)
                timer2.ExpireFromNow(snet::Seconds(1));

            if (timer2_times == 3)
                cancelled_timer.Cancel();
            else if (timer2_times > 3)
                event_loop->Stop();
        });

    event_loop->AddLoopHandler(&timer_driver);
    event_loop->Loop();

    return 0;
}
