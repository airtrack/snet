#include "MessageQueue.h"
#include <stdio.h>
#include <thread>
#include <memory>

class Consumer
{
public:
    Consumer()
        : thread_(&Consumer::ConsumerThreadFunc, this)
    {
    }

    ~Consumer()
    {
        thread_.join();
    }

    Consumer(const Consumer &) = delete;
    void operator = (const Consumer &) = delete;

    void Send(int v)
    {
        msg_queue_.Send(std::unique_ptr<int>(new int(v)));
    }

    void SendNull()
    {
        msg_queue_.Send(std::unique_ptr<int>());
    }

private:
    void ConsumerThreadFunc()
    {
        while (true)
        {
            auto msg = msg_queue_.Recv();
            if (!msg)
                break;

            auto v = *msg;
            printf("Recv msg %d\n", v);
        }

        while (true)
        {
            auto msg = msg_queue_.TryRecv();
            if (msg)
            {
                auto v = std::move(*msg);
                if (!v)
                    break;

                printf("TryRecv msg %d\n", *v);
            }
        }
    }

    snet::MessageQueue<std::unique_ptr<int>> msg_queue_;
    std::thread thread_;
};

int main()
{
    Consumer c;

    for (int i = 0; i < 10; ++i)
        c.Send(i);

    c.SendNull();

    for (int i = 0; i < 10; ++i)
        c.Send(i);

    c.SendNull();

    return 0;
}
