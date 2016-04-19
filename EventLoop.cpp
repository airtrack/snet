#include "EventLoop.h"
#include "KQueue.h"

namespace snet
{

std::unique_ptr<EventLoop> CreateEventLoop()
{
    return std::unique_ptr<EventLoop>(new KQueue);
}

} // namespace snet
