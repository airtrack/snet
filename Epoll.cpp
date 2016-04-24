#include "Epoll.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

namespace snet
{

Epoll::Epoll()
    : epoll_fd_(epoll_create(1)),
      stop_(false)
{
}

Epoll::~Epoll()
{
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

void Epoll::AddEventHandler(EventHandler *eh)
{
    SetEpollEvents(EPOLL_CTL_ADD, eh);
}

void Epoll::DelEventHandler(EventHandler *eh)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));

    auto fd = eh->Fd();
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &event);
}

void Epoll::UpdateEvents(EventHandler *eh)
{
    SetEpollEvents(EPOLL_CTL_MOD, eh);
}

void Epoll::SetEpollEvents(int op, EventHandler *eh)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));

    auto fd = eh->Fd();
    auto events = static_cast<int>(eh->EnabledEvents());

    if (events & static_cast<int>(Event::Read))
        event.events |= EPOLLIN;

    if (events & static_cast<int>(Event::Write))
        event.events |= EPOLLOUT;

    event.data.ptr = eh;

    epoll_ctl(epoll_fd_, op, fd, &event);
}

void Epoll::AddLoopHandler(LoopHandler *lh)
{
    lh_set_.AddLoopHandler(lh);
}

void Epoll::DelLoopHandler(LoopHandler *lh)
{
    lh_set_.DelLoopHandler(lh);
}

void Epoll::Loop()
{
    while (!stop_)
    {
        struct epoll_event events[10];
        auto num = epoll_wait(epoll_fd_, events, 10, 20);

        for (int i = 0; i < num; ++i)
        {
            auto eh = static_cast<EventHandler *>(events[i].data.ptr);

            if (events[i].events & EPOLLIN)
                eh->HandleRead();
            if (events[i].events & EPOLLOUT)
                eh->HandleWrite();
        }

        lh_set_.HandleLoop();
    }

    lh_set_.HandleStop();
}

void Epoll::Stop()
{
    stop_ = true;
}

} // namespace snet
