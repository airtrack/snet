#ifndef STUNNEL_H
#define STUNNEL_H

#include "Buffer.h"
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <memory>

namespace stunnel
{

enum class Protocol : unsigned char
{
    Unknown = 0,
    Open,
    Close,
    Data
};

inline unsigned long long Htonll(unsigned long long n)
{
    return static_cast<unsigned long long>(
        htonl(static_cast<unsigned int>(n))) << 32 |
        htonl(static_cast<unsigned int>(n >> 32));
}

inline unsigned long long Ntohll(unsigned long long n)
{
    return Htonll(n);
}

inline std::size_t GetProtocolHeadSize()
{
    return sizeof(char) + sizeof(unsigned long long);
}

inline std::size_t GetOpenProtocolSize(const std::string &host)
{
    return GetProtocolHeadSize() + host.size() + sizeof(unsigned short);
}

inline std::size_t GetCloseProtocolSize()
{
    return GetProtocolHeadSize();
}

inline std::size_t GetDataProtocolSize(std::size_t data_size)
{
    return GetProtocolHeadSize() + data_size;
}

inline std::unique_ptr<snet::Buffer> PackOpen(unsigned long long id,
                                              const std::string &host,
                                              unsigned short port)
{
    auto size = GetOpenProtocolSize(host);
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[size], size, snet::OpDeleter));

    auto buf = buffer->buf;
    *buf++ = static_cast<unsigned char>(Protocol::Open);

    *reinterpret_cast<unsigned long long *>(buf) = Htonll(id);
    buf += sizeof(id);

    memcpy(buf, host.data(), host.size());
    buf += host.size();

    *reinterpret_cast<unsigned short *>(buf) = htons(port);
    return buffer;
}

inline Protocol UnpackProtocolType(snet::Buffer &buffer)
{
    if (buffer.pos >= buffer.size)
        return Protocol::Unknown;

    unsigned char c = *(buffer.buf + buffer.pos);
    buffer.pos += 1;

    if (c == static_cast<unsigned char>(Protocol::Open))
        return Protocol::Open;
    else if (c == static_cast<unsigned char>(Protocol::Close))
        return Protocol::Close;
    else if (c == static_cast<unsigned char>(Protocol::Data))
        return Protocol::Data;
    else
        return Protocol::Unknown;
}

inline unsigned long long UnpackId(snet::Buffer &buffer)
{
    auto id = Ntohll(*reinterpret_cast<unsigned long long *>(
            buffer.buf + buffer.pos));
    buffer.pos += sizeof(id);
    return id;
}

inline bool UnpackOpen(snet::Buffer &buffer, unsigned long long *id,
                       std::string *host, unsigned short *port)
{
    if (buffer.pos + GetOpenProtocolSize("") >= buffer.size)
        return false;

    *id = UnpackId(buffer);

    auto len = buffer.size - buffer.pos - sizeof(*port);
    host->assign(buffer.buf + buffer.pos, buffer.buf + buffer.pos + len);
    buffer.pos += len;

    *port = ntohs(*reinterpret_cast<unsigned short *>(buffer.buf + buffer.pos));
    buffer.pos += sizeof(*port);

    return true;
}

} // namespace stunnel

#endif // STUNNEL_H
