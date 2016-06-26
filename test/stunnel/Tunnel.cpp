#include "Tunnel.h"
#include <arpa/inet.h>
#include <string.h>

namespace tunnel
{

#define VERIFY_DATA "#&^@!~-=`"

// Clang needs link static const which referenced in lambda, make clang happy.
const int Connection::kAliveSeconds = 60;
const int Connection::kHeartbeatSeconds = 5;

Connection::Connection(std::unique_ptr<snet::Connection> connection,
                       const std::string &key, snet::TimerList *timer_list,
                       State state)
    : state_(state),
      recv_length_buffer_(recv_length_, sizeof(recv_length_)),
      alive_timer_(timer_list),
      heartbeat_timer_(timer_list),
      encryptor_(key.data(), key.size()),
      decryptor_(key.data(), key.size()),
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

void Connection::Handshake(const OnHandshakeOk &oh)
{
    if (state_ == State::Connecting)
    {
        on_handshake_ok_ = oh;
        if (SetupEncryptor())
        {
            std::unique_ptr<snet::Buffer> data(
                new snet::Buffer(new char[sizeof(VERIFY_DATA)],
                                 sizeof(VERIFY_DATA), snet::OpDeleter));
            memcpy(data->buf, VERIFY_DATA, sizeof(VERIFY_DATA));
            SendEncryptBuffer(encryptor_.Encrypt(std::move(data)));
        }
    }
}

void Connection::Send(std::unique_ptr<snet::Buffer> buffer)
{
    if (state_ == State::Running)
        SendEncryptBuffer(encryptor_.Encrypt(std::move(buffer)));
}

bool Connection::SendEncryptBuffer(std::unique_ptr<snet::Buffer> buffer)
{
    std::unique_ptr<snet::Buffer> length(
        new snet::Buffer(new char[kLengthBytes],
                         kLengthBytes, snet::OpDeleter));

    auto size = buffer->size;
    *reinterpret_cast<unsigned short *>(length->buf) = htons(size);

    if (SendBuffer(std::move(length)))
        return SendBuffer(std::move(buffer));
    else
        return false;
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
        {
            recv_length_buffer_.pos += ret;
            alive_timer_.ExpireFromNow(snet::Seconds(kAliveSeconds));
        }
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
            {
                recv_buffer_->pos += ret;
                alive_timer_.ExpireFromNow(snet::Seconds(kAliveSeconds));
            }

            if (recv_buffer_->pos == recv_buffer_->size)
            {
                recv_buffer_->pos = 0;
                recv_length_buffer_.pos = 0;

                if (state_ == State::Running)
                    data_handler_(decryptor_.Decrypt(std::move(recv_buffer_)));
                else
                    HandleHandshake();
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

void Connection::HandleHandshake()
{
    switch (state_)
    {
    case State::Accepting:
        if (SetupDecryptor())
            state_ = State::AcceptingPhase2;
        break;

    case State::AcceptingPhase2:
        {
            auto data = decryptor_.Decrypt(std::move(recv_buffer_));
            if (data->size != sizeof(VERIFY_DATA))
                return error_handler_();

            if (memcmp(data->buf, VERIFY_DATA, data->size) != 0)
                return error_handler_();

            if (SetupEncryptor())
                state_ = State::Running;
        }
        break;

    case State::Connecting:
        if (SetupDecryptor())
        {
            state_ = State::Running;
            on_handshake_ok_();
        }
        break;

    default:
        error_handler_();
        break;
    }
}

bool Connection::SetupEncryptor()
{
    cipher::IVec ivec;
    ivec.RandomReset();

    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(reinterpret_cast<char *>(ivec.ivec),
                         sizeof(ivec.ivec)));

    auto encrypt_buffer = encryptor_.Encrypt(std::move(buffer));
    encryptor_.SetIVec(ivec);

    return SendEncryptBuffer(std::move(encrypt_buffer));
}

bool Connection::SetupDecryptor()
{
    auto ivec = decryptor_.Decrypt(std::move(recv_buffer_));
    if (ivec->size != sizeof(cipher::IVec::ivec))
    {
        error_handler_();
        return false;
    }

    cipher::IVec iv;
    memcpy(iv.ivec, ivec->buf, sizeof(iv.ivec));
    decryptor_.SetIVec(iv);
    return true;
}

Client::Client(const std::string &ip, unsigned short port,
               const std::string &key, snet::EventLoop *loop,
               snet::TimerList *timer_list)
    : key_(key),
      timer_list_(timer_list),
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

void Client::Send(std::unique_ptr<snet::Buffer> buffer)
{
    if (connection_)
        connection_->Send(std::move(buffer));
}

void Client::HandleConnect(std::unique_ptr<snet::Connection> connection,
                           const OnConnected &onc)
{
    if (connection)
    {
        connection_.reset(new Connection(std::move(connection),
                                         key_, timer_list_,
                                         Connection::State::Connecting));
        connection_->SetErrorHandler(error_handler_);
        connection_->SetDataHandler(data_handler_);
        connection_->Handshake(onc);
    }
    else
    {
        error_handler_();
    }
}

Server::Server(const std::string &ip, unsigned short port,
               const std::string &key, snet::EventLoop *loop,
               snet::TimerList *timer_list)
    : key_(key),
      acceptor_(ip, port, loop),
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
        new Connection(std::move(connection), key_, timer_list_,
                       Connection::State::Accepting)));
}

} // namespace tunnel
