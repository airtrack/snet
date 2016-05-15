#include "Acceptor.h"
#include "Connection.h"
#include "EventLoop.h"
#include "SocketOps.h"
#include "MessageQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <thread>
#include <set>

class Worker final
{
public:
    Worker()
        : connection_getter_(this),
          thread_(&Worker::ThreadFunc, this)
    {
    }

    Worker(const Worker &) = delete;
    void operator = (const Worker &) = delete;

    ~Worker()
    {
        thread_.join();
    }

    void AddConnection(std::unique_ptr<snet::Connection> connection)
    {
        message_queue_.Send(std::move(connection));
    }

private:
    using ConnectionPtr = std::shared_ptr<snet::Connection>;
    using ConnectionSet = std::set<ConnectionPtr>;
    using MessageQueue = snet::MessageQueue<std::unique_ptr<snet::Connection>>;

    class ConnectionGetter final : public snet::LoopHandler
    {
    public:
        explicit ConnectionGetter(Worker *worker)
            : worker_(worker)
        {
        }

        virtual void HandleLoop() override
        {
            auto msg = worker_->message_queue_.TryRecv();
            if (msg)
                worker_->AddNewConnection(std::move(*msg));
        }

        virtual void HandleStop() override { }

    private:
        Worker *worker_;
    };

    void ThreadFunc()
    {
        event_loop_ = snet::CreateEventLoop();
        event_loop_->AddLoopHandler(&connection_getter_);
        event_loop_->Loop();
    }

    void AddNewConnection(std::unique_ptr<snet::Connection> connection)
    {
        connection->ChangeEventLoop(event_loop_.get());
        ConnectionPtr c(std::move(connection));
        connection_set_.insert(c);

        std::weak_ptr<snet::Connection> w(c);
        c->SetOnError(
            [this, w] () {
                ConnectionError(w.lock());
            });
        c->SetOnReceivable(
            [this, w] () {
                ConnectionRecv(w.lock());
            });
    }

    void ConnectionError(const ConnectionPtr &c)
    {
        c->Close();
        connection_set_.erase(c);
    }

    void ConnectionRecv(const ConnectionPtr &c)
    {
        char buf[16 * 1024];
        snet::Buffer buffer(buf, sizeof(buf));
        auto ret = c->Recv(&buffer);

        if (ret == static_cast<int>(snet::RecvE::PeerClosed) ||
            ret == static_cast<int>(snet::RecvE::Error))
        {
            c->Close();
            connection_set_.erase(c);
            return ;
        }

        if (ret == static_cast<int>(snet::RecvE::NoAvailData))
            return ;

        buffer.pos = ret;
        if (buffer.pos > 0)
        {
            char *send_buf = new char[buffer.pos];
            memcpy(send_buf, buffer.buf, buffer.pos);

            std::unique_ptr<snet::Buffer> send_buffer(
                new snet::Buffer(send_buf, buffer.pos, SendBufferDeleter));

            if (c->Send(std::move(send_buffer)) ==
                static_cast<int>(snet::SendE::Error))
            {
                c->Close();
                connection_set_.erase(c);
            }
        }
    }

    static void SendBufferDeleter(snet::Buffer *buffer)
    {
        delete [] buffer->buf;
    }

    MessageQueue message_queue_;
    ConnectionSet connection_set_;
    ConnectionGetter connection_getter_;
    std::unique_ptr<snet::EventLoop> event_loop_;
    std::thread thread_;
};

class Server final
{
public:
    Server(snet::EventLoop *loop, int worker_num)
        : worker_num_(worker_num),
          next_worker_(0),
          loop_(loop)
    {
    }

    bool Listen(const char *ip, unsigned short port)
    {
        acceptor_.reset(new snet::Acceptor(ip, port, loop_, 128));
        if (!acceptor_->IsListenOk())
            return false;

        acceptor_->SetNewConnectionWithEventLoop(false);
        acceptor_->SetOnNewConnection(
            [this] (std::unique_ptr<snet::Connection> connection) {
                AddNewConnection(std::move(connection));
            });

        for (int i = 0; i < worker_num_; ++i)
        {
            WorkerPtr worker(new Worker);
            worker_list_.push_back(std::move(worker));
        }
        return true;
    }

private:
    using WorkerPtr = std::unique_ptr<Worker>;
    using WorkerList = std::vector<WorkerPtr>;

    void AddNewConnection(std::unique_ptr<snet::Connection> connection)
    {
        auto index = next_worker_++;
        next_worker_ = next_worker_ % worker_num_;
        worker_list_[index]->AddConnection(std::move(connection));
    }

    int worker_num_;
    int next_worker_;
    snet::EventLoop *loop_;
    WorkerList worker_list_;
    std::unique_ptr<snet::Acceptor> acceptor_;
};

int main(int argc, const char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s listen_ip port worker_thread\n", argv[0]);
        return 1;
    }

    auto threads = atoi(argv[3]);
    if (threads <= 0)
    {
        fprintf(stderr, "Invalid thread number %d, use 1 thread\n", threads);
        threads = 1;
    }

    int max_files = 1024;
    if (!snet::SetMaxOpenFiles(max_files))
        fprintf(stderr, "Change max open files to %d failed\n", max_files);

    auto event_loop = snet::CreateEventLoop();
    Server server(event_loop.get(), threads);

    if (server.Listen(argv[1], atoi(argv[2])))
        event_loop->Loop();

    return 0;
}
