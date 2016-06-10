#include "Epoll.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

namespace snet
{

Epoll::Epoll()
    : stop_(false),
      epoll_fd_(epoll_create(1)),
      events_(new struct epoll_event[kMaxEvents])
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

    for (int i = 0; i < kMaxEvents; ++i)
    {
        if (events_[i].data.ptr == eh)
            events_[i].data.ptr = nullptr;
    }
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
        auto num = epoll_wait(epoll_fd_, events_.get(), kMaxEvents, 20);

        for (int i = 0; i < num; ++i)
        {
            if (events_[i].events & EPOLLIN)
            {
                auto eh = static_cast<EventHandler *>(events_[i].data.ptr);
                if (eh)
                    eh->HandleRead();
            }

            if (events_[i].events & EPOLLOUT)
            {
                auto eh = static_cast<EventHandler *>(events_[i].data.ptr);
                if (eh)
                    eh->HandleWrite();
            }
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
