#include "Connector.h"
#include "SocketOps.h"
#include <errno.h>

namespace snet
{

Connector::Connector(const std::string &ip, unsigned short port,
                     EventLoop *loop)
    : fd_(-1),
      eh_registered_(false),
      ip_(ip),
      port_(port),
      loop_(loop),
      eh_(this)
{
}

Connector::~Connector()
{
    if (eh_registered_)
        loop_->DelEventHandler(&eh_);

    if (fd_ >= 0)
        close(fd_);
}

void Connector::Connect(const OnConnected &oc)
{
    oc_ = oc;

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
        return ConnectFailed();

    if (!SetSocketNonBlock(fd_))
        return ConnectFailed();

    struct sockaddr_in sin;
    SetSockAddrIn(&sin, ip_.c_str(), port_);

    int ret = connect(fd_, reinterpret_cast<struct sockaddr *>(&sin),
                      sizeof(sin));
    if (ret == 0)
        return ConnectSucceeded();

    if (errno != EINPROGRESS)
        return ConnectFailed();

    ProcessInProgress();
}

void Connector::ProcessInProgress()
{
    loop_->AddEventHandler(&eh_);
    eh_registered_ = true;
}

void Connector::HandleConnect()
{
    loop_->DelEventHandler(&eh_);
    eh_registered_ = false;

    int error = 0;
    socklen_t len = sizeof(error);

    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0)
    {
        if (!error)
            return ConnectSucceeded();
    }

    ConnectFailed();
}

void Connector::ConnectFailed()
{
    oc_(ConnectionPtr());
}

void Connector::ConnectSucceeded()
{
    ConnectionPtr connection(new Connection(fd_, loop_));
    fd_ = -1;

    oc_(std::move(connection));
}

} // namespace snet
