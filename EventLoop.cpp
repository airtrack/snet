#include "EventLoop.h"
#include "KQueue.h"
#include "Epoll.h"

namespace snet
{

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
