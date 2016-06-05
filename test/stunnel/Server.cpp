#include "STunnel.h"
#include "Tunnel.h"
#include "Relay.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class STunnelConnection final
{
public:
    using ErrorHandler = tunnel::Connection::ErrorHandler;

    STunnelConnection(std::unique_ptr<tunnel::Connection> connection,
                      snet::EventLoop *loop,
                      snet::AddrInfoResolver *addrinfo_resolver)
        : loop_(loop),
          addrinfo_resolver_(addrinfo_resolver),
          tunnel_(std::move(connection))
    {
        tunnel_->SetDataHandler(
            [this] (std::unique_ptr<snet::Buffer> data) {
                HandleTunnelData(std::move(data));
            });
    }

    STunnelConnection(const STunnelConnection &) = delete;
    void operator = (const STunnelConnection &) = delete;

    void SetErrorHandler(const ErrorHandler &error_handler)
    {
        tunnel_->SetErrorHandler(error_handler);
    }

private:
    void HandleTunnelData(std::unique_ptr<snet::Buffer> data)
    {
        auto proto = stunnel::UnpackProtocolType(*data);
        switch (proto)
        {
        case stunnel::Protocol::Open:
            HandleOpen(std::move(data));
            break;

        case stunnel::Protocol::Close:
            HandleClose(std::move(data));
            break;

        case stunnel::Protocol::Data:
            HandleData(std::move(data));
            break;

        default:
            break;
        }
    }

    void HandleOpen(std::unique_ptr<snet::Buffer> data)
    {
        std::string host;
        unsigned short port = 0;
        unsigned long long id = 0;

        if (!stunnel::UnpackOpen(*data, &id, &host, &port))
            return ;

        std::unique_ptr<relay::Client> rclient(
            new relay::Client(port, loop_, addrinfo_resolver_));

        rclient->SetEventHandler(
            [this, id] (relay::Client::Event event) {
                HandleRelayEvent(id, event);
            });
        rclient->SetDataHandler(
            [this, id] (std::unique_ptr<snet::Buffer> data) {
                HandleRelayData(id, std::move(data));
            });
        rclient->ConnectHost(host);

        relays_.emplace(id, std::move(rclient));
    }

    void HandleClose(std::unique_ptr<snet::Buffer> data)
    {
        unsigned long long id = 0;
        if (stunnel::UnpackClose(*data, &id))
            relays_.erase(id);
    }

    void HandleData(std::unique_ptr<snet::Buffer> data)
    {
        unsigned long long id = 0;
        if (stunnel::UnpackData(*data, &id))
        {
            auto it = relays_.find(id);
            if (it == relays_.end())
                tunnel_->Send(stunnel::PackClose(id));
            else
                it->second->Send(std::move(data));
        }
    }

    void HandleRelayEvent(unsigned long long id,
                          relay::Client::Event event)
    {
        if (event == relay::Client::Event::ConnectServerSuccess)
        {
            auto it = relays_.find(id);
            if (it == relays_.end())
                tunnel_->Send(stunnel::PackClose(id));
            else
            {
                struct sockaddr_in inet;
                it->second->GetPeerAddress(&inet);
                tunnel_->Send(
                    stunnel::PackOpenSuccess(
                        id, ntohl(inet.sin_addr.s_addr),
                        ntohs(inet.sin_port)));
            }
        }
        else
        {
            tunnel_->Send(stunnel::PackClose(id));
            relays_.erase(id);
        }
    }

    void HandleRelayData(unsigned long long id,
                         std::unique_ptr<snet::Buffer> data)
    {
        tunnel_->Send(stunnel::PackData(id, *data));
    }

    snet::EventLoop *loop_;
    snet::AddrInfoResolver *addrinfo_resolver_;

    std::unique_ptr<tunnel::Connection> tunnel_;
    std::unordered_map<unsigned long long,
        std::unique_ptr<relay::Client>> relays_;
};

class STunnelServer final
{
public:
    STunnelServer(const std::string &ip, unsigned short port,
                  snet::EventLoop *loop, snet::TimerList *timer_list,
                  snet::AddrInfoResolver *addrinfo_resolver)
        : loop_(loop),
          addrinfo_resolver_(addrinfo_resolver),
          server_(ip, port, loop, timer_list)
    {
        server_.SetOnNewConnection(
            [this] (std::unique_ptr<tunnel::Connection> connection) {
                NewConnection(
                    std::make_shared<STunnelConnection>(
                        std::move(connection), loop_, addrinfo_resolver_));
            });
    }

    STunnelServer(const STunnelServer &) = delete;
    void operator = (const STunnelServer &) = delete;

    bool IsListenOk() const
    {
        return server_.IsListenOk();
    }

private:
    using TunnelPtr = std::shared_ptr<STunnelConnection>;

    void NewConnection(TunnelPtr tunnel)
    {
        tunnels_.insert(tunnel);

        std::weak_ptr<STunnelConnection> weak(tunnel);
        tunnel->SetErrorHandler(
            [this, weak] () {
                tunnels_.erase(weak.lock());
            });
    }

    snet::EventLoop *loop_;
    snet::AddrInfoResolver *addrinfo_resolver_;

    tunnel::Server server_;
    std::unordered_set<TunnelPtr> tunnels_;
};

int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s IP Port\n", argv[0]);
        return 1;
    }

    auto event_loop = snet::CreateEventLoop();
    snet::TimerList timer_list;
    snet::AddrInfoResolver addrinfo_resolver;

    STunnelServer server(argv[1], atoi(argv[2]),
                         event_loop.get(),
                         &timer_list,
                         &addrinfo_resolver);
    if (!server.IsListenOk())
    {
        fprintf(stderr, "Listen %s:%s error\n", argv[1], argv[2]);
        return 1;
    }

    snet::TimerDriver timer_driver(timer_list);
    event_loop->AddLoopHandler(&timer_driver);
    event_loop->AddLoopHandler(&addrinfo_resolver);
    event_loop->Loop();

    return 0;
}
