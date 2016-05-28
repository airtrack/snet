#include "Tunnel.h"
#include <arpa/inet.h>
#include <string.h>

namespace tunnel
{

// Clang needs link static const which referenced in lambda, make clang happy.
const int Connection::kAliveSeconds = 15;
const int Connection::kHeartbeatSeconds = 5;

Connection::Connection(std::unique_ptr<snet::Connection> connection,
                       snet::TimerList *timer_list)
    : recv_length_buffer_(recv_length_, sizeof(recv_length_)),
      alive_timer_(timer_list),
      heartbeat_timer_(timer_list),
      connection_(std::move(connection))
{
    memset(heartbeat_, 0, sizeof(heartbeat_));
    memset(recv_length_, 0, sizeof(recv_length_));

    connection_->SetOnError([this] () { HandleError(); });
    connection_->SetOnReceivable([this] () { HandleReceivable(); });

    alive_timer_.ExpireFromNow(snet::Seconds(kAliveSeconds));
    heartbeat_timer_.ExpireFromNow(snet::Seconds(kHeartbeatSeconds));

    alive_timer_.SetOnTimeout([this] () { HandleAliveTimeout(); });
    heartbeat_timer_.SetOnTimeout([this] () { HandleHeartbeat(); });
}

void Connection::SetErrorHandler(const ErrorHandler &error_handler)
{
    error_handler_ = error_handler;
}

void Connection::SetDataHandler(const DataHandler &data_handler)
{
    data_handler_ = data_handler;
}

void Connection::Send(std::unique_ptr<snet::Buffer> buffer)
{
    std::unique_ptr<snet::Buffer> length(
        new snet::Buffer(new char[kLengthBytes],
                         kLengthBytes, snet::OpDeleter));

    auto size = buffer->size;
    *reinterpret_cast<unsigned short *>(length->buf) = htons(size);

    if (SendBuffer(std::move(length)))
        SendBuffer(std::move(buffer));
}

bool Connection::SendBuffer(std::unique_ptr<snet::Buffer> buffer)
{
    if (connection_->Send(std::move(buffer)) ==
        static_cast<int>(snet::SendE::Error))
    {
        error_handler_();
        return false;
    }

    return true;
}

void Connection::HandleError()
{
    error_handler_();
}

void Connection::HandleReceivable()
{
    if (recv_length_buffer_.pos < recv_length_buffer_.size)
    {
        auto ret = connection_->Recv(&recv_length_buffer_);
        if (ret == static_cast<int>(snet::RecvE::PeerClosed) ||
            ret == static_cast<int>(snet::RecvE::Error))
        {
            error_handler_();
            return ;
        }

        if (ret > 0)
            recv_length_buffer_.pos += ret;
    }

    if (recv_length_buffer_.pos == recv_length_buffer_.size)
    {
        if (!recv_buffer_)
        {
            auto length = ntohs(
                *reinterpret_cast<unsigned short *>(recv_length_buffer_.buf));
            if (length == 0)
            {
                // Heartbeat
                alive_timer_.ExpireFromNow(snet::Seconds(kAliveSeconds));
                recv_length_buffer_.pos = 0;
            }
            else
            {
                auto data = new char[length];
                recv_buffer_.reset(
                    new snet::Buffer(data, length, snet::OpDeleter));
            }
        }

        if (recv_buffer_)
        {
            auto ret = connection_->Recv(recv_buffer_.get());
            if (ret == static_cast<int>(snet::RecvE::PeerClosed) ||
                ret == static_cast<int>(snet::RecvE::Error))
            {
                error_handler_();
                return ;
            }

            if (ret > 0)
                recv_buffer_->pos += ret;
            if (recv_buffer_->pos == recv_buffer_->size)
            {
                recv_buffer_->pos = 0;
                recv_length_buffer_.pos = 0;
                data_handler_(std::move(recv_buffer_));
            }
        }
    }
}

void Connection::HandleAliveTimeout()
{
    error_handler_();
}

void Connection::HandleHeartbeat()
{
    heartbeat_timer_.ExpireFromNow(snet::Seconds(kHeartbeatSeconds));

    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(heartbeat_, sizeof(heartbeat_)));
    SendBuffer(std::move(buffer));
}

Client::Client(const std::string &ip, unsigned short port,
               snet::EventLoop *loop, snet::TimerList *timer_list)
    : timer_list_(timer_list),
      connector_(ip, port, loop)
{
}

void Client::SetErrorHandler(const ErrorHandler &error_handler)
{
    error_handler_ = error_handler;
}

void Client::SetDataHandler(const DataHandler &data_handler)
{
    data_handler_ = data_handler;
}

void Client::Connect(const OnConnected &onc)
{
    connector_.Connect(
        [this, onc] (std::unique_ptr<snet::Connection> connection) {
            HandleConnect(std::move(connection), onc);
        });
}

void Client::HandleConnect(std::unique_ptr<snet::Connection> connection,
                           const OnConnected &onc)
{
    if (connection)
    {
        connection_.reset(new Connection(std::move(connection), timer_list_));
        connection_->SetErrorHandler(error_handler_);
        connection_->SetDataHandler(data_handler_);

        onc();
    }
    else
    {
        error_handler_();
    }
}

Server::Server(const std::string &ip, unsigned short port,
               snet::EventLoop *loop, snet::TimerList *timer_list)
    : acceptor_(ip, port, loop),
      timer_list_(timer_list)
{
    acceptor_.SetOnNewConnection(
        [this] (std::unique_ptr<snet::Connection> connection) {
            HandleNewConnection(std::move(connection));
        });
}

bool Server::IsListenOk() const
{
    return acceptor_.IsListenOk();
}

void Server::SetOnNewConnection(const OnNewConnection &onc)
{
    onc_ = onc;
}

void Server::HandleNewConnection(std::unique_ptr<snet::Connection> connection)
{
    onc_(std::unique_ptr<Connection>(
        new Connection(std::move(connection), timer_list_)));
}

} // namespace tunnel
