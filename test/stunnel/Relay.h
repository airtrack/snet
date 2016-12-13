#ifndef RELAY_H
#define RELAY_H

#include "EventLoop.h"
#include "Connector.h"
#include "Connection.h"
#include "AddrInfoResolver.h"
#include <functional>
#include <memory>
#include <string>
#include <queue>

namespace relay
{

class Client final
{
public:
    enum class Event : int
    {
        AddrInfoResolveFail = 1,
        ConnectServerFail,
        ConnectServerSuccess,
        ConnectionError,
        PeerClosed,
        RecvError,
        SendError,
    };

    using EventHandler = std::function<void (Event)>;
    using DataHandler = std::function<void (std::unique_ptr<snet::Buffer>)>;

    Client(unsigned short port, snet::EventLoop *loop,
           snet::AddrInfoResolver *addrinfo_resolver);
    ~Client();

    Client(const Client &) = delete;
    void operator = (const Client &) = delete;

    void SetEventHandler(const EventHandler &event_handler);
    void SetDataHandler(const DataHandler &data_handler);
    void ConnectHost(const std::string &host);
    bool GetPeerAddress(struct sockaddr_in *inet);
    void Send(std::unique_ptr<snet::Buffer> buffer);
    void ShutdownWrite();

private:
    void Connect(const snet::AddrInfoResolver::SockAddrs &addrs);
    void Connect();
    void HandleConnect(std::unique_ptr<snet::Connection> connection);
    void HandleReceivable();

    static const std::size_t kBufferSize = 2048;

    snet::EventLoop *loop_;
    snet::AddrInfoResolver *addrinfo_resolver_;
    const snet::AddrInfoResolver::Request *request_;

    std::unique_ptr<snet::Connector> connector_;
    std::unique_ptr<snet::Connection> connection_;

    std::string ip_;
    unsigned short port_;
    EventHandler event_handler_;
    DataHandler data_handler_;
    std::queue<struct sockaddr_in> addrs_;
};

} // namespace relay

#endif // RELAY_H
