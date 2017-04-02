#ifndef SNET_ENDIAN_H
#define SNET_ENDIAN_H

#ifdef __APPLE__

#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)

#elif defined(__linux__)

#include <endian.h>

#endif

#include <arpa/inet.h>
#include <stdint.h>

namespace snet
{

inline uint16_t HostToNet16(uint16_t host16)
{
    return htons(host16);
}

inline uint32_t HostToNet32(uint32_t host32)
{
    return htonl(host32);
}

inline uint64_t HostToNet64(uint64_t host64)
{
    return htobe64(host64);
}

inline uint16_t NetToHost16(uint16_t net16)
{
    return ntohs(net16);
}

inline uint32_t NetToHost32(uint32_t net32)
{
    return ntohl(net32);
}

inline uint64_t NetToHost64(uint64_t net64)
{
    return be64toh(net64);
}

} // namespace snet

#endif // SNET_ENDIAN_H
