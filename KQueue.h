#ifndef KQUEUE_H
#define KQUEUE_H

#include "EventLoop.h"
#include <memory>

struct kevent;

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
    static const int kMaxEvents = 10;

    bool stop_;
    int kqueue_fd_;
    LoopHandlerSet lh_set_;
    std::unique_ptr<struct kevent []> events_;
};

} // namespace snet

#endif // KQUEUE_H
