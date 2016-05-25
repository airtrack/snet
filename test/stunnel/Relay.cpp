#include "Relay.h"
#include "SocketOps.h"

namespace relay
{

Client::Client(unsigned short port, snet::EventLoop *loop,
               snet::AddrInfoResolver *addrinfo_resolver)
    : loop_(loop),
      addrinfo_resolver_(addrinfo_resolver),
      port_(port)
{
}

void Client::SetEventHandler(const EventHandler &event_handler)
{
    event_handler_ = event_handler;
}

void Client::SetDataHandler(const DataHandler &data_handler)
{
    data_handler_ = data_handler;
}

void Client::ConnectHost(const std::string &host)
{
    addrinfo_resolver_->AsyncResolve(
        host, [this] (const snet::AddrInfoResolver::SockAddrs &addrs) {
            if (addrs.empty())
                event_handler_(Event::AddrInfoResolveFail);
            else
                Connect(addrs);
        });
}

void Client::Send(std::unique_ptr<snet::Buffer> buffer)
{
    connection_->Send(std::move(buffer));
}

void Client::Connect(const snet::AddrInfoResolver::SockAddrs &addrs)
{
    for (auto addr : addrs)
    {
        if (addr->sa_family == AF_INET)
        {
            const struct sockaddr_in *in =
                reinterpret_cast<const struct sockaddr_in *>(addr);
            addrs_.push(*in);
        }
    }

    Connect();
}

void Client::Connect()
{
    if (addrs_.empty())
        return event_handler_(Event::ConnectServerFail);

    ip_ = snet::SockAddrToString(addrs_.front());
    addrs_.pop();

    connector_.reset(new snet::Connector(ip_, port_, loop_));
    connector_->Connect([this] (std::unique_ptr<snet::Connection> connection) {
                            if (connection)
                                HandleConnect(std::move(connection));
                            else
                                Connect();
                        });
}

void Client::HandleConnect(std::unique_ptr<snet::Connection> connection)
{
    connection_ = std::move(connection);
    connection_->SetOnError(
        [this] () { event_handler_(Event::ConnectionError); });
    connection_->SetOnReceivable(
        [this] () { HandleReceivable(); });
}

void Client::HandleReceivable()
{
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[kBufferSize], kBufferSize));

    auto ret = connection_->Recv(buffer.get());
    if (ret == static_cast<int>(snet::RecvE::PeerClosed))
        event_handler_(Event::PeerClosed);
    else if (ret == static_cast<int>(snet::RecvE::Error))
        event_handler_(Event::RecvError);
    else if (ret != static_cast<int>(snet::RecvE::NoAvailData))
    {
        buffer->pos = ret;
        data_handler_(std::move(buffer));
    }
}

} // namespace relay
