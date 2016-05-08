#ifndef SOCKET_OPS_H
#define SOCKET_OPS_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace snet
{

bool SetSocketNonBlock(int fd);

void SetSocketReuseAddr(int fd);

void SetSockAddrIn(struct sockaddr_in *sin,
                   const char *ip, unsigned short port);

bool SetMaxOpenFiles(rlim_t num);

} // namespace snet

#endif // SOCKET_OPS_H
