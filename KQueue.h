#ifndef KQUEUE_H
#define KQUEUE_H

#include "EventLoop.h"

namespace snet
{

class KQueue final : public EventLoop
{
public:
    KQueue();
    ~KQueue();

    virtual void AddEventHandler(EventHandler *eh) override;
    virtual void DelEventHandler(EventHandler *eh) override;
    virtual void UpdateEvents(EventHandler *eh) override;
    virtual void Loop() override;

private:
    int kqueue_fd_;
};

} // namespace snet

#endif // KQUEUE_H
