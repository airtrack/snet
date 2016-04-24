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
    virtual void AddLoopHandler(LoopHandler *lh) override;
    virtual void DelLoopHandler(LoopHandler *lh) override;
    virtual void Loop() override;
    virtual void Stop() override;

private:
    int kqueue_fd_;
    bool stop_;
    LoopHandlerSet lh_set_;
};

} // namespace snet

#endif // KQUEUE_H
