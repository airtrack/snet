#include "Connector.h"
#include "Connection.h"
#include "EventLoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <vector>

class Client final
{
public:
    explicit Client(snet::EventLoop *loop)
        : loop_(loop),
          send_buf_(100),
          recv_buf_(100),
          recv_(&recv_buf_[0], recv_buf_.size())
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
            return loop_->Stop();

        connection_->SetOnError(
            [this] (){
                connection_->Close();
                loop_->Stop();
            });
        connection_->SetOnReceivable(
            [this] () {
                Recv();
            });

        Send();
    }

    void Send()
    {
        std::unique_ptr<snet::Buffer> buffer(
            new snet::Buffer(&send_buf_[0], send_buf_.size()));

        if (connection_->Send(std::move(buffer)) ==
            static_cast<int>(snet::Send::Error))
        {
            connection_->Close();
            loop_->Stop();
        }
    }

    void Recv()
    {
        auto ret = connection_->Recv(&recv_);
        if (ret == static_cast<int>(snet::Recv::PeerClosed) ||
            ret == static_cast<int>(snet::Recv::Error))
        {
            connection_->Close();
            return loop_->Stop();
        }

        if (ret == static_cast<int>(snet::Recv::NoAvailData))
            return ;

        recv_.pos += ret;
        if (recv_.pos == recv_.size)
        {
            recv_.pos = 0;
            send_buf_ = recv_buf_;
            Send();
        }
    }

    snet::EventLoop *loop_;
    std::vector<char> send_buf_;
    std::vector<char> recv_buf_;
    snet::Buffer recv_;

    std::unique_ptr<snet::Connector> connector_;
    std::unique_ptr<snet::Connection> connection_;
};

int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s server_ip port\n", argv[0]);
        return 1;
    }

    auto event_loop = snet::CreateEventLoop();
    Client client(event_loop.get());

    client.Connect(argv[1], atoi(argv[2]));

    event_loop->Loop();
    return 0;
}
