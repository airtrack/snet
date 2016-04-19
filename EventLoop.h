#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <memory>

namespace snet
{

enum class Event : int
{
    Read = 1,
    Write = 2
};

class EventHandler
{
public:
    virtual ~EventHandler() { }
    virtual int Fd() const = 0;
    virtual Event Events() const = 0;
    virtual Event EnabledEvents() const = 0;
    virtual void HandleRead() = 0;
    virtual void HandleWrite() = 0;
};

class EventLoop
{
public:
    EventLoop() { }
    EventLoop(const EventLoop &) = delete;
    void operator = (const EventLoop &) = delete;

    virtual ~EventLoop() { }
    virtual void AddEventHandler(EventHandler *eh) = 0;
    virtual void DelEventHandler(EventHandler *eh) = 0;
    virtual void UpdateEvents(EventHandler *eh) = 0;
    virtual void Loop() = 0;
    virtual void Stop() = 0;
};

std::unique_ptr<EventLoop> CreateEventLoop();

} // namespace snet

#endif // EVENT_LOOP_H
