#include "EventLoop.h"
#include "KQueue.h"
#include "Epoll.h"

namespace snet
{

LoopHandlerSet::LoopHandlerSet()
{
}

void LoopHandlerSet::AddLoopHandler(LoopHandler *lh)
{
    set_.insert(lh);
}

void LoopHandlerSet::DelLoopHandler(LoopHandler *lh)
{
    set_.erase(lh);
}

void LoopHandlerSet::HandleLoop()
{
    for (auto lh : set_)
        lh->HandleLoop();
}

void LoopHandlerSet::HandleStop()
{
    for (auto lh : set_)
        lh->HandleStop();
}

std::unique_ptr<EventLoop> CreateEventLoop()
{
#ifdef __APPLE__
    return std::unique_ptr<EventLoop>(new KQueue);
#elif __linux__
    return std::unique_ptr<EventLoop>(new Epoll);
#else
#error "Platform is not support"
#endif
}

} // namespace snet
