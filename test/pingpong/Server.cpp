#include "Acceptor.h"
#include "Connection.h"
#include "EventLoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <set>

class Server final
{
public:
    explicit Server(snet::EventLoop *loop)
        : loop_(loop)
    {
    }

    bool Listen(const char *ip, unsigned short port)
    {
        acceptor_.reset(new snet::Acceptor(ip, port, loop_));
        if (!acceptor_->IsListenOk())
            return false;

        acceptor_->SetOnNewConnection(
            [this] (std::unique_ptr<snet::Connection> connection) {
                AddNewConnection(std::move(connection));
            });
        return true;
    }

private:
    using ConnectionPtr = std::shared_ptr<snet::Connection>;
    using ConnectionSet = std::set<ConnectionPtr>;

    void AddNewConnection(std::unique_ptr<snet::Connection> connection)
    {
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
        char buf[512];
        snet::Buffer buffer(buf, sizeof(buf));
        auto ret = c->Recv(&buffer);

        if (ret == static_cast<int>(snet::Recv::PeerClosed) ||
            ret == static_cast<int>(snet::Recv::Error))
        {
            c->Close();
            connection_set_.erase(c);
            return ;
        }

        if (ret == static_cast<int>(snet::Recv::NoAvailData))
            return ;

        buffer.pos = ret;
        if (buffer.pos > 0)
        {
            char *send_buf = new char[buffer.pos];
            memcpy(send_buf, buffer.buf, buffer.pos);

            std::unique_ptr<snet::Buffer> send_buffer(
                new snet::Buffer(send_buf, buffer.pos, SendBufferDeleter));

            if (c->Send(std::move(send_buffer)) ==
                static_cast<int>(snet::Send::Error))
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

    snet::EventLoop *loop_;
    ConnectionSet connection_set_;
    std::unique_ptr<snet::Acceptor> acceptor_;
};

int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s listen_ip port\n", argv[0]);
        return 1;
    }

    auto event_loop = snet::CreateEventLoop();
    Server server(event_loop.get());

    if (server.Listen(argv[1], atoi(argv[2])))
        event_loop->Loop();

    return 0;
}
