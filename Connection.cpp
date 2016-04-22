#include "Connection.h"
#include "SocketOps.h"
#include <errno.h>

namespace snet
{

Connection::Connection(int fd, EventLoop *loop)
    : fd_(fd),
      loop_(loop),
      eh_(this)
{
    loop_->AddEventHandler(&eh_);
}

Connection::~Connection()
{
    Close();
}

int Connection::Send(std::unique_ptr<Buffer> buffer)
{
    if (!send_queue_.empty())
    {
        send_queue_.push(std::move(buffer));
        return static_cast<int>(SendE::OK);
    }

    auto ret = WriteBuffer(buffer);
    if (ret == static_cast<int>(SendE::Error))
        return ret;

    if (buffer->pos == buffer->size)
        return ret;

    send_queue_.push(std::move(buffer));

    eh_.EnableWrite();
    loop_->UpdateEvents(&eh_);
    return ret;
}

int Connection::Recv(Buffer *buffer)
{
    auto buf = buffer->buf + buffer->pos;
    auto len = buffer->size - buffer->pos;
    auto bytes = recv(fd_, buf, len, 0);

    if (bytes == 0)
    {
        eh_.DisableRead();
        loop_->UpdateEvents(&eh_);
        return static_cast<int>(RecvE::PeerClosed);
    }

    if (bytes < 0 && errno != EAGAIN && errno != EINTR)
        return static_cast<int>(RecvE::Error);

    if (bytes < 0)
        return static_cast<int>(RecvE::NoAvailData);
    return bytes;
}

void Connection::Close()
{
    if (fd_ >= 0)
    {
        loop_->DelEventHandler(&eh_);
        close(fd_);
        fd_ = -1;
    }
}

void Connection::SetOnError(const OnError &oe)
{
    on_error_ = oe;
}

void Connection::SetOnReceivable(const OnReceivable &onr)
{
    on_recv_ = onr;
}

int Connection::WriteBuffer(const std::unique_ptr<Buffer> &buffer)
{
    auto buf = buffer->buf + buffer->pos;
    auto len = buffer->size - buffer->pos;
    auto bytes = send(fd_, buf, len, 0);

    if (bytes < 0 && errno != EAGAIN && errno != EINTR)
        return static_cast<int>(SendE::Error);

    if (bytes < 0)
        bytes = 0;

    buffer->pos += bytes;
    return static_cast<int>(SendE::OK);
}

void Connection::HandleRead()
{
    on_recv_();
}

void Connection::HandleWrite()
{
    while (!send_queue_.empty())
    {
        auto &buffer = send_queue_.front();
        auto ret = WriteBuffer(buffer);

        if (ret == static_cast<int>(SendE::Error))
        {
            eh_.DisableWrite();
            loop_->UpdateEvents(&eh_);

            on_error_();
            break;
        }

        if (buffer->pos < buffer->size)
            break;

        send_queue_.pop();
    }

    if (send_queue_.empty())
    {
        eh_.DisableWrite();
        loop_->UpdateEvents(&eh_);
    }
}

} // namespace snet
