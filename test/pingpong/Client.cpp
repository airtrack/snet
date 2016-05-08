#include "Connector.h"
#include "Connection.h"
#include "EventLoop.h"
#include "SocketOps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <vector>

class Client final
{
public:
    explicit Client(snet::EventLoop *loop)
        : loop_(loop)
    {
    }

    static void SetClientNumber(int num)
    {
        client_number = num;
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
            [this] (){
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
        if (--client_number == 0)
            loop_->Stop();
    }

    static void SendBufferDeleter(snet::Buffer *buffer)
    {
        delete [] buffer->buf;
    }

    static const std::size_t kDataSize = 16 * 1024;

    static int client_number;

    snet::EventLoop *loop_;
    std::unique_ptr<snet::Connector> connector_;
    std::unique_ptr<snet::Connection> connection_;
};

int Client::client_number = 0;

int main(int argc, const char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s server_ip port client_num\n", argv[0]);
        return 1;
    }

    auto client_num = atoi(argv[3]);
    if (client_num <= 0)
    {
        fprintf(stderr, "Invalid client number %d, use 1 as default\n",
                client_num);
        client_num = 1;
    }

    int max_files = 1024;
    if (!snet::SetMaxOpenFiles(max_files))
        fprintf(stderr, "Change max open files to %d failed\n", max_files);

    auto event_loop = snet::CreateEventLoop();
    std::vector<std::unique_ptr<Client>> clients;

    Client::SetClientNumber(client_num);
    for (int i = 0; i < client_num; ++i)
    {
        std::unique_ptr<Client> client(new Client(event_loop.get()));
        client->Connect(argv[1], atoi(argv[2]));
        clients.push_back(std::move(client));
    }

    event_loop->Loop();
    return 0;
}
