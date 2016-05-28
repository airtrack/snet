#ifndef TUNNEL_H
#define TUNNEL_H

#include "Acceptor.h"
#include "Connector.h"
#include "Connection.h"
#include "EventLoop.h"
#include "Timer.h"

namespace tunnel
{

class Connection final
{
public:
    using ErrorHandler = std::function<void ()>;
    using DataHandler = std::function<void (std::unique_ptr<snet::Buffer>)>;

    Connection(std::unique_ptr<snet::Connection> connection,
               snet::TimerList *timer_list);

    Connection(const Connection &) = delete;
    void operator = (const Connection &) = delete;

    void SetErrorHandler(const ErrorHandler &error_handler);
    void SetDataHandler(const DataHandler &data_handler);
    void Send(std::unique_ptr<snet::Buffer> buffer);

private:
    bool SendBuffer(std::unique_ptr<snet::Buffer> buffer);
    void HandleError();
    void HandleReceivable();
    void HandleAliveTimeout();
    void HandleHeartbeat();

    static const int kLengthBytes = 2;
    static const int kAliveSeconds;
    static const int kHeartbeatSeconds;

    char heartbeat_[kLengthBytes];
    char recv_length_[kLengthBytes];

    snet::Buffer recv_length_buffer_;
    std::unique_ptr<snet::Buffer> recv_buffer_;

    snet::Timer alive_timer_;
    snet::Timer heartbeat_timer_;

    ErrorHandler error_handler_;
    DataHandler data_handler_;
    std::unique_ptr<snet::Connection> connection_;
};

class Client final
{
public:
    using OnConnected = std::function<void ()>;
    using ErrorHandler = Connection::ErrorHandler;
    using DataHandler = Connection::DataHandler;

    Client(const std::string &ip, unsigned short port,
           snet::EventLoop *loop, snet::TimerList *timer_list);

    Client(const Client &) = delete;
    void operator = (const Client &) = delete;

    void SetErrorHandler(const ErrorHandler &error_handler);
    void SetDataHandler(const DataHandler &data_handler);
    void Connect(const OnConnected &onc);

private:
    void HandleConnect(std::unique_ptr<snet::Connection> connection,
                       const OnConnected &onc);

    snet::TimerList *timer_list_;
    snet::Connector connector_;

    ErrorHandler error_handler_;
    DataHandler data_handler_;
    std::unique_ptr<Connection> connection_;
};

class Server final
{
public:
    using OnNewConnection = std::function<void (std::unique_ptr<Connection>)>;

    Server(const std::string &ip, unsigned short port,
           snet::EventLoop *loop, snet::TimerList *timer_list);

    Server(const Server &) = delete;
    void operator = (const Server &) = delete;

    bool IsListenOk() const;
    void SetOnNewConnection(const OnNewConnection &onc);

private:
    void HandleNewConnection(std::unique_ptr<snet::Connection> connection);

    OnNewConnection onc_;
    snet::Acceptor acceptor_;
    snet::TimerList *timer_list_;
};

} // namespace tunnel

#endif // TUNNEL_H
