#include "SocketOps.h"

namespace snet
{

bool SetSocketNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return false;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return false;

    return true;
}

void SetSocketReuseAddr(int fd)
{
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

void SetSockAddrIn(struct sockaddr_in *sin,
                   const char *ip, unsigned short port)
{
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = inet_addr(ip);
}

std::string SockAddrToString(const struct sockaddr &addr)
{
    if (addr.sa_family == AF_INET)
    {
        auto inet = reinterpret_cast<const struct sockaddr_in *>(&addr);
        return SockAddrToString(*inet);
    }
    else if (addr.sa_family == AF_INET6)
    {
        auto inet6 = reinterpret_cast<const struct sockaddr_in6 *>(&addr);
        return SockAddrToString(*inet6);
    }

    return std::string();
}

std::string SockAddrToString(const struct sockaddr_in &addr_in)
{
    char ipv4[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &addr_in.sin_addr, ipv4, sizeof(ipv4));
    return std::string(ipv4);
}

std::string SockAddrToString(const struct sockaddr_in6 &addr_in6)
{
    char ipv6[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET6, &addr_in6.sin6_addr, ipv6, sizeof(ipv6));
    return std::string(ipv6);
}

bool SetMaxOpenFiles(rlim_t num)
{
    struct rlimit files;
    if (getrlimit(RLIMIT_NOFILE, &files) < 0)
        return false;

    auto rlim_max = files.rlim_max;
    files.rlim_max = num > rlim_max ? num : rlim_max;
    files.rlim_cur = num;

    if (setrlimit(RLIMIT_NOFILE, &files) < 0)
    {
        if (errno == EPERM)
        {
            files.rlim_max = rlim_max;
            files.rlim_cur = num > rlim_max ? rlim_max : num;
            return setrlimit(RLIMIT_NOFILE, &files) == 0;
        }

        return false;
    }

    return true;
}

} // namespace snet
