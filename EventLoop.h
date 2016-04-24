#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <memory>
#include <set>

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

class LoopHandler
{
public:
    virtual ~LoopHandler() { }
    virtual void HandleLoop() = 0;
    virtual void HandleStop() = 0;
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
    virtual void AddLoopHandler(LoopHandler *lh) = 0;
    virtual void DelLoopHandler(LoopHandler *lh) = 0;
    virtual void Loop() = 0;
    virtual void Stop() = 0;
};

class LoopHandlerSet final
{
public:
    LoopHandlerSet();

    LoopHandlerSet(const LoopHandlerSet &) = delete;
    void operator = (const LoopHandlerSet &) = delete;

    void AddLoopHandler(LoopHandler *lh);
    void DelLoopHandler(LoopHandler *lh);
    void HandleLoop();
    void HandleStop();

private:
    std::set<LoopHandler *> set_;
};

std::unique_ptr<EventLoop> CreateEventLoop();

} // namespace snet

#endif // EVENT_LOOP_H
