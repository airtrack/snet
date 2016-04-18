#ifndef SOCKET_OPS_H
#define SOCKET_OPS_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

namespace snet
{

inline bool SetSocketNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return false;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return false;

    return true;
}

inline void SetSockAddrIn(struct sockaddr_in *sin,
                          const char *ip, unsigned short port)
{
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = inet_addr(ip);
}

} // namespace snet

#endif // SOCKET_OPS_H
