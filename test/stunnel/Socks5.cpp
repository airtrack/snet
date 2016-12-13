#include "Socks5.h"

namespace socks5
{

#define VERSION 5
#define METHOD_NO_AUTH 0
#define METHOD_NO_ACCEPT 0xFF

#define CMD_CONNECT 1
#define RSV 0
#define ATYP_IPV4 1
#define ATYP_DOMAIN_NAME 3

#define REPLY_SUCCESS 0
#define REPLY_FAILURE 1

Connection::Connection(std::unique_ptr<snet::Connection> connection)
    : state_(State::SelectingMethod),
      connection_(std::move(connection))
{
    connection_->SetOnError([this] () { HandleError(); });
    connection_->SetOnReceivable([this] () { HandleRecv(); });
}

void Connection::SetOnClose(const OnErrorClose &on_error_close)
{
    on_error_close_ = on_error_close;
}

void Connection::SetOnConnectAddress(const OnConnectAddress &oca)
{
    on_connect_address_ = oca;
}

void Connection::SetOnEof(const OnEof &on_eof)
{
    on_eof_ = on_eof;
}

void Connection::SetDataHandler(const DataHandler &data_handler)
{
    data_handler_ = data_handler;
}

void Connection::ReplyConnectSuccess(unsigned int ip, unsigned short port)
{
    if (state_ == State::Connecting)
    {
        state_ = State::Running;
        ReplyConnectResult(ip, port);
    }
}

void Connection::Send(std::unique_ptr<snet::Buffer> data)
{
    if (state_ == State::Running)
        connection_->Send(std::move(data));
}

void Connection::ShutdownWrite()
{
    if (state_ == State::Running)
        connection_->Shutdown(snet::ShutdownT::Write);
}

void Connection::Close()
{
    if (state_ == State::Connecting)
        ReplyConnectResult(0, 0);

    CloseConnection();
}

void Connection::ReplyConnectResult(unsigned int ip, unsigned short port)
{
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[kReplySize], kReplySize, snet::OpDeleter));

    auto buf = buffer->buf;
    *buf++ = VERSION;
    *buf++ = ip != 0 ? REPLY_SUCCESS : REPLY_FAILURE;
    *buf++ = RSV;
    *buf++ = ATYP_IPV4;

    *reinterpret_cast<unsigned int *>(buf) = htonl(ip);
    buf += sizeof(ip);

    *reinterpret_cast<unsigned short *>(buf) = htons(port);

    connection_->Send(std::move(buffer));
}

void Connection::CloseConnection()
{
    connection_->Close();
    state_ = State::Closed;
}

void Connection::HandleError()
{
    CloseConnection();
    on_error_close_();
}

void Connection::HandleRecv()
{
    if (state_ == State::SelectingMethod)
        SelectMethod();
    else if (state_ == State::GettingConnectAddress)
        GetConnectAddress();
    else
        RecvData();
}

void Connection::SelectMethod()
{
    if (!buffer_)
    {
        buffer_.reset(new snet::Buffer(new char[kMaxSelectMethodSize],
                                       kMaxSelectMethodSize, snet::OpDeleter));
    }

    auto ret = connection_->Recv(buffer_.get());
    if (ret <= 0)
        return HandleError();

    buffer_->pos += ret;
    if (buffer_->pos >= 2)
    {
        if (buffer_->buf[0] != VERSION)
            return HandleError();

        std::size_t num = static_cast<unsigned char>(buffer_->buf[1]);
        if (buffer_->pos > num + 2)
            return HandleError();
        if (buffer_->pos < num + 2)
            return ;

        for (std::size_t i = 0; i < num; ++i)
        {
            if (buffer_->buf[i + 2] == METHOD_NO_AUTH)
                return ReplyMethod(METHOD_NO_AUTH);
        }

        ReplyMethod(METHOD_NO_ACCEPT);
        HandleError();
    }
}

void Connection::ReplyMethod(unsigned char method)
{
    buffer_.reset(new snet::Buffer(new char[kReplyMethodSize],
                                   kReplyMethodSize, snet::OpDeleter));

    auto buf = buffer_->buf;
    *buf++ = VERSION;
    *buf = method;

    if (method == METHOD_NO_AUTH)
        state_ = State::GettingConnectAddress;

    connection_->Send(std::move(buffer_));
}

void Connection::GetConnectAddress()
{
    if (!buffer_)
    {
        buffer_.reset(new snet::Buffer(
                new char[kGetConnectAddressSize],
                kGetConnectAddressSize, snet::OpDeleter));
    }

    auto ret = connection_->Recv(buffer_.get());
    if (ret <= 0)
        return HandleError();

    buffer_->pos += ret;
    if (buffer_->pos >= 4)
    {
        if (buffer_->buf[1] != CMD_CONNECT)
            return HandleError();
        if (buffer_->buf[3] != ATYP_DOMAIN_NAME)
            return HandleError();

        if (buffer_->pos == 4)
            return ;

        std::size_t len = static_cast<unsigned char>(buffer_->buf[4]);
        if (buffer_->pos > len + 4 + 2 + 1)
            return HandleError();
        if (buffer_->pos < len + 4 + 2 + 1)
            return ;

        auto buf = buffer_->buf + 5;
        std::string domain_name(buf, buf + len);

        buf += len;
        auto port = ntohs(*reinterpret_cast<unsigned short *>(buf));

        buffer_.reset();
        state_ = State::Connecting;
        on_connect_address_(std::move(domain_name), port);
    }
}

void Connection::RecvData()
{
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[kBufferSize], kBufferSize, snet::OpDeleter));

    auto ret = connection_->Recv(buffer.get());
    if (ret == static_cast<int>(snet::RecvE::NoAvailData))
        return ;

    if (ret == static_cast<int>(snet::RecvE::PeerClosed))
        return on_eof_();

    if (ret <= 0)
        return HandleError();

    buffer->size = ret;
    data_handler_(std::move(buffer));
}

Server::Server(const std::string &ip, unsigned short port,
               snet::EventLoop *loop)
    : enable_accept_(true),
      acceptor_(ip, port, loop)
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

void Server::DisableAccept()
{
    enable_accept_ = false;
}

void Server::EnableAccept()
{
    enable_accept_ = true;
}

void Server::HandleNewConnection(std::unique_ptr<snet::Connection> connection)
{
    if (enable_accept_)
    {
        onc_(std::unique_ptr<Connection>(
                new Connection(std::move(connection))));
    }
}

} // namespace socks5
