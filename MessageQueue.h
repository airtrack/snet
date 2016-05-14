#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <queue>
#include <mutex>
#include <memory>
#include <utility>
#include <condition_variable>

namespace snet
{

template<typename T>
class MessageQueue
{
public:
    using Type = T;

    MessageQueue() { }
    MessageQueue(const MessageQueue &) = delete;
    void operator = (const MessageQueue &) = delete;

    template<typename U>
    void Send(U &&u)
    {
        {
            std::lock_guard<std::mutex> l(mutex_);
            queue_.push(std::forward<U>(u));
        }
        cond_var_.notify_one();
    }

    T Recv()
    {
        T t;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this] () { return !queue_.empty(); });
            t = std::move(queue_.front());
            queue_.pop();
        }

        return std::move(t);
    }

    std::unique_ptr<T> TryRecv()
    {
        std::unique_ptr<T> t;

        {
            std::lock_guard<std::mutex> l(mutex_);
            if (!queue_.empty())
            {
                t.reset(new T(std::move(queue_.front())));
                queue_.pop();
            }
        }

        return std::move(t);
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
};

} // namespace snet

#endif // MESSAGE_QUEUE_H
