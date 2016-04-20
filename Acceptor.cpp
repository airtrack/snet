#include "Acceptor.h"
#include "SocketOps.h"

namespace snet
{

Acceptor::Acceptor(const std::string &ip, unsigned short port,
                   EventLoop *loop)
    : fd_(-1),
      listen_ok_(false),
      loop_(loop),
      eh_(this)
{
    if (CreateListenSocket(ip, port))
        loop_->AddEventHandler(&eh_);
}

Acceptor::~Acceptor()
{
    loop_->DelEventHandler(&eh_);

    if (fd_ >= 0)
        close(fd_);
}

bool Acceptor::IsListenOk() const
{
    return listen_ok_;
}

void Acceptor::SetOnNewConnection(const OnNewConnection &onc)
{
    onc_ = onc;
}

bool Acceptor::CreateListenSocket(const std::string &ip, unsigned short port)
{
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
        return false;

    if (!SetSocketNonBlock(fd_))
        return false;

    SetSocketReuseAddr(fd_);

    struct sockaddr_in sin;
    SetSockAddrIn(&sin, ip.c_str(), port);

    if (bind(fd_, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)) < 0)
        return false;

    if (listen(fd_, 5) < 0)
        return false;

    listen_ok_ = true;
    return true;
}

void Acceptor::HandleAccept()
{
    struct sockaddr addr;
    socklen_t len = sizeof(addr);

    int new_fd = accept(fd_, &addr, &len);
    if (new_fd < 0)
        return ;

    if (!SetSocketNonBlock(new_fd))
    {
        close(new_fd);
        return ;
    }

    onc_(ConnectionPtr(new Connection(new_fd, loop_)));
}

} // namespace snet
