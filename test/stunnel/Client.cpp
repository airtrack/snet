#include "STunnel.h"
#include "Tunnel.h"
#include "Socks5.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <unordered_map>

#define SOCKS5_LISTEN_IP "127.0.0.1"
#define SOCKS5_LISTEN_PORT 1080

class Server final
{
public:
    Server(const std::string &tunnel_ip, unsigned short tunnel_port,
           const std::string &tunnel_key, snet::EventLoop *loop,
           snet::TimerList *timer_list)
        : id_generator_(0),
          tunnel_port_(tunnel_port),
          tunnel_ip_(tunnel_ip),
          tunnel_key_(tunnel_key),
          loop_(loop),
          timer_list_(timer_list),
          server_(SOCKS5_LISTEN_IP, SOCKS5_LISTEN_PORT, loop),
          tunnel_reconnect_timer_(timer_list)
    {
        server_.DisableAccept();
        server_.SetOnNewConnection(
            [this] (std::unique_ptr<socks5::Connection> connection) {
                HandleSocks5NewConn(std::move(connection));
            });

        CreateTunnel();
    }

    Server(const Server &) = delete;
    void operator = (const Server &) = delete;

    bool IsListenOk() const
    {
        return server_.IsListenOk();
    }

private:
    void HandleSocks5NewConn(std::unique_ptr<socks5::Connection> conn)
    {
        auto id = ++id_generator_;
        conn->SetOnClose(
            [this, id] () {
                HandleSocks5ConnClose(id);
            });
        conn->SetDataHandler(
            [this, id] (std::unique_ptr<snet::Buffer> data) {
                HandleSocks5ConnData(id, std::move(data));
            });
        conn->SetOnConnectAddress(
            [this, id] (std::string domain_name, unsigned short port) {
                HandleSocks5ConnAddress(id, std::move(domain_name), port);
            });

        socks5_conns_.emplace(id, std::move(conn));
    }

    void HandleSocks5ConnClose(unsigned long long id)
    {
        tunnel_->Send(stunnel::PackClose(id));
        socks5_conns_.erase(id);
    }

    void HandleSocks5ConnData(unsigned long long id,
                              std::unique_ptr<snet::Buffer> data)
    {
        tunnel_->Send(stunnel::PackData(id, *data));
    }

    void HandleSocks5ConnAddress(unsigned long long id,
                                 std::string domain_name,
                                 unsigned short port)
    {
        tunnel_->Send(stunnel::PackOpen(id, domain_name, port));
    }

    void CreateTunnel()
    {
        tunnel_.reset(new tunnel::Client(tunnel_ip_, tunnel_port_,
                                         tunnel_key_, loop_, timer_list_));
        tunnel_->SetErrorHandler(
            [this] () {
                HandleTunnelError();
            });
        tunnel_->SetDataHandler(
            [this] (std::unique_ptr<snet::Buffer> data) {
                HandleTunnelData(std::move(data));
            });
        tunnel_->Connect(
            [this] () {
                HandleTunnelConnected();
            });
    }

    void HandleTunnelError()
    {
        server_.DisableAccept();
        socks5_conns_.clear();

        tunnel_reconnect_timer_.ExpireFromNow(snet::Seconds(1));
        tunnel_reconnect_timer_.SetOnTimeout([this] () { CreateTunnel(); });
    }

    void HandleTunnelData(std::unique_ptr<snet::Buffer> data)
    {
        auto proto = stunnel::UnpackProtocolType(*data);

        switch (proto)
        {
        case stunnel::Protocol::OpenSuccess:
            {
                unsigned int ip = 0;
                unsigned short port = 0;
                unsigned long long id = 0;
                if (stunnel::UnpackOpenSuccess(*data, &id, &ip, &port))
                {
                    auto it = socks5_conns_.find(id);
                    if (it != socks5_conns_.end())
                        it->second->ReplyConnectSuccess(ip, port);
                }
            }
            break;

        case stunnel::Protocol::Data:
            {
                unsigned long long id = 0;
                if (stunnel::UnpackData(*data, &id))
                {
                    auto it = socks5_conns_.find(id);
                    if (it != socks5_conns_.end())
                        it->second->Send(std::move(data));
                }
            }
            break;

        case stunnel::Protocol::Close:
            {
                unsigned long long id = 0;
                if (stunnel::UnpackClose(*data, &id))
                {
                    auto it = socks5_conns_.find(id);
                    if (it != socks5_conns_.end())
                    {
                        it->second->Close();
                        socks5_conns_.erase(it);
                    }
                }
            }
            break;

        default:
            break;
        }
    }

    void HandleTunnelConnected()
    {
        server_.EnableAccept();
    }

    unsigned long long id_generator_;
    unsigned short tunnel_port_;
    std::string tunnel_ip_;
    std::string tunnel_key_;

    snet::EventLoop *loop_;
    snet::TimerList *timer_list_;

    std::unordered_map<unsigned long long,
        std::unique_ptr<socks5::Connection>> socks5_conns_;

    socks5::Server server_;
    snet::Timer tunnel_reconnect_timer_;
    std::unique_ptr<tunnel::Client> tunnel_;
};

int main(int argc, const char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s ServerIP Port Key\n", argv[0]);
        return 1;
    }

    auto event_loop = snet::CreateEventLoop();
    snet::TimerList timer_list;

    Server server(argv[1], atoi(argv[2]), argv[3],
                  event_loop.get(), &timer_list);

    if (!server.IsListenOk())
    {
        fprintf(stderr, "Listen address %s:%d error\n",
                SOCKS5_LISTEN_IP, SOCKS5_LISTEN_PORT);
        return 1;
    }

    snet::TimerDriver timer_driver(timer_list);
    event_loop->AddLoopHandler(&timer_driver);
    event_loop->Loop();

    return 0;
}
