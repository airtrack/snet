#include "Connector.h"
#include "Connection.h"
#include "EventLoop.h"
#include "SocketOps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <vector>
#include <thread>

class Client final
{
public:
    Client(snet::EventLoop *loop, int *client_counter)
        : client_counter_(client_counter),
          loop_(loop)
    {
    }

    void Connect(const char *ip, unsigned short port)
    {
        connector_.reset(new snet::Connector(ip, port, loop_));
        connector_->Connect(
            [this] (std::unique_ptr<snet::Connection> connection) {
                connection_ = std::move(connection);
                Run();
            });
    }

private:
    void Run()
    {
        if (!connection_)
            return Stop();

        connection_->SetOnError(
            [this] () {
                connection_->Close();
                Stop();
            });
        connection_->SetOnReceivable(
            [this] () {
                Recv();
            });

        char *buf = new char[kDataSize];
        std::unique_ptr<snet::Buffer> buffer(
            new snet::Buffer(buf, kDataSize, SendBufferDeleter));
        Send(std::move(buffer));
    }

    void Send(std::unique_ptr<snet::Buffer> buffer)
    {
        if (connection_->Send(std::move(buffer)) ==
            static_cast<int>(snet::SendE::Error))
        {
            connection_->Close();
            Stop();
        }
    }

    void Recv()
    {
        char buf[kDataSize];
        snet::Buffer buffer(buf, sizeof(buf));
        auto ret = connection_->Recv(&buffer);

        if (ret == static_cast<int>(snet::RecvE::PeerClosed) ||
            ret == static_cast<int>(snet::RecvE::Error))
        {
            connection_->Close();
            return Stop();
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
            Send(std::move(send_buffer));
        }
    }

    void Stop()
    {
        if (--*client_counter_ == 0)
            loop_->Stop();
    }

    static void SendBufferDeleter(snet::Buffer *buffer)
    {
        delete [] buffer->buf;
    }

    static const std::size_t kDataSize = 16 * 1024;

    int *client_counter_;
    snet::EventLoop *loop_;
    std::unique_ptr<snet::Connector> connector_;
    std::unique_ptr<snet::Connection> connection_;
};

class Worker final
{
public:
    Worker(const char *ip, unsigned short port, int client_num)
        : client_num_(client_num),
          thread_(&Worker::ThreadFunc, this, ip, port)
    {
    }

    Worker(const Worker &) = delete;
    void operator = (const Worker &) = delete;

    ~Worker()
    {
        thread_.join();
    }

private:
    void ThreadFunc(const char *ip, unsigned short port)
    {
        event_loop_ = snet::CreateEventLoop();

        for (int i = 0; i < client_num_; ++i)
        {
            std::unique_ptr<Client> client(
                new Client(event_loop_.get(), &client_num_));
            client->Connect(ip, port);
            clients_.push_back(std::move(client));
        }

        event_loop_->Loop();
    }

    int client_num_;
    std::unique_ptr<snet::EventLoop> event_loop_;
    std::vector<std::unique_ptr<Client>> clients_;
    std::thread thread_;
};

int main(int argc, const char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s server_ip port clients_per_thread threads\n",
                argv[0]);
        return 1;
    }

    auto client_num = atoi(argv[3]);
    if (client_num <= 0)
    {
        fprintf(stderr, "Invalid client number %d, use 1 as default\n",
                client_num);
        client_num = 1;
    }

    auto threads = atoi(argv[4]);
    if (threads <= 0)
    {
        fprintf(stderr, "Invalid thread nubmer %d, use 1 thread\n", threads);
        threads = 1;
    }

    int max_files = 1024;
    if (!snet::SetMaxOpenFiles(max_files))
        fprintf(stderr, "Change max open files to %d failed\n", max_files);

    std::vector<std::unique_ptr<Worker>> worker_list;

    for (int i = 0; i < threads; ++i)
    {
        std::unique_ptr<Worker> worker(
            new Worker(argv[1], atoi(argv[2]), client_num));
        worker_list.push_back(std::move(worker));
    }

    return 0;
}
