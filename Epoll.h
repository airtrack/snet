#ifndef EPOLL_H
#define EPOLL_H

#include "EventLoop.h"

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
    virtual void Loop() override;
    virtual void Stop() override;

private:
    void SetEpollEvents(int op, EventHandler *eh);

    int epoll_fd_;
    bool stop_;
};

} // namespace snet

#endif // EPOLL_H
