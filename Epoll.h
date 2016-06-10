#ifndef EPOLL_H
#define EPOLL_H

#include "EventLoop.h"
#include <memory>

struct epoll_event;

namespace snet
{

class Epoll final : public EventLoop
{
public:
    Epoll();
    ~Epoll();

    virtual void AddEventHandler(EventHandler *eh) override;
    virtual void DelEventHandler(EventHandler *eh) override;
    virtual void UpdateEvents(EventHandler *eh) override;
    virtual void AddLoopHandler(LoopHandler *lh) override;
    virtual void DelLoopHandler(LoopHandler *lh) override;
    virtual void Loop() override;
    virtual void Stop() override;

private:
    void SetEpollEvents(int op, EventHandler *eh);

    static const int kMaxEvents = 10;

    bool stop_;
    int epoll_fd_;
    LoopHandlerSet lh_set_;
    std::unique_ptr<struct epoll_event []> events_;
};

} // namespace snet

#endif // EPOLL_H
