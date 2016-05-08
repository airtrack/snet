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
