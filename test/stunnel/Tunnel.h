#ifndef TUNNEL_H
#define TUNNEL_H

#include "EventLoop.h"
#include "Connection.h"
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

} // namespace tunnel

#endif // TUNNEL_H
