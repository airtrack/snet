#include "Relay.h"
#include "SocketOps.h"

namespace relay
{

Client::Client(unsigned short port, snet::EventLoop *loop,
               snet::AddrInfoResolver *addrinfo_resolver)
    : loop_(loop),
      addrinfo_resolver_(addrinfo_resolver),
      request_(nullptr),
      port_(port)
{
}

Client::~Client()
{
    if (request_)
        addrinfo_resolver_->CancelRequest(request_);
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
    request_ = addrinfo_resolver_->AsyncResolve(
        host, [this] (const snet::AddrInfoResolver::SockAddrs &addrs) {
            request_ = nullptr;
            if (addrs.empty())
                event_handler_(Event::AddrInfoResolveFail);
            else
                Connect(addrs);
        });
}

bool Client::GetPeerAddress(struct sockaddr_in *inet)
{
    if (connection_)
        return connection_->GetPeerAddress(inet);
    return false;
}

void Client::Send(std::unique_ptr<snet::Buffer> buffer)
{
    if (connection_->Send(std::move(buffer)) ==
        static_cast<int>(snet::SendE::Error))
        event_handler_(Event::SendError);
}

void Client::ShutdownWrite()
{
    connection_->Shutdown(snet::ShutdownT::Write);
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

    event_handler_(Event::ConnectServerSuccess);
}

void Client::HandleReceivable()
{
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[kBufferSize], kBufferSize, snet::OpDeleter));

    auto ret = connection_->Recv(buffer.get());
    if (ret == static_cast<int>(snet::RecvE::PeerClosed))
        event_handler_(Event::PeerClosed);
    else if (ret == static_cast<int>(snet::RecvE::Error))
        event_handler_(Event::RecvError);
    else if (ret != static_cast<int>(snet::RecvE::NoAvailData))
    {
        buffer->size = ret;
        data_handler_(std::move(buffer));
    }
}

} // namespace relay
