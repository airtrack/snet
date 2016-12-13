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
    OpenSuccess,
    ShutdownWrite,
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

inline std::size_t GetOpenSuccessProtocolSize()
{
    return GetProtocolHeadSize() + sizeof(unsigned int)
        + sizeof(unsigned short);
}

inline std::size_t GetCloseProtocolSize()
{
    return GetProtocolHeadSize();
}

inline std::size_t GetShutdownWriteProtocolSize()
{
    return GetProtocolHeadSize();
}

inline std::size_t GetDataProtocolSize(std::size_t data_size)
{
    return GetProtocolHeadSize() + data_size;
}

inline std::unique_ptr<snet::Buffer> PrepareBufferAndPackHead(
    std::size_t size, Protocol protocol, unsigned long long id)
{
    std::unique_ptr<snet::Buffer> buffer(
        new snet::Buffer(new char[size], size, snet::OpDeleter));

    auto buf = buffer->buf;
    *buf++ = static_cast<unsigned char>(protocol);

    *reinterpret_cast<unsigned long long *>(buf) = Htonll(id);
    return buffer;
}

inline std::unique_ptr<snet::Buffer> PackOpen(unsigned long long id,
                                              const std::string &host,
                                              unsigned short port)
{
    auto size = GetOpenProtocolSize(host);
    auto buffer = PrepareBufferAndPackHead(size, Protocol::Open, id);
    auto buf = buffer->buf + GetProtocolHeadSize();

    memcpy(buf, host.data(), host.size());
    buf += host.size();

    *reinterpret_cast<unsigned short *>(buf) = htons(port);
    return buffer;
}

inline std::unique_ptr<snet::Buffer> PackOpenSuccess(unsigned long long id,
                                                     unsigned int ip,
                                                     unsigned short port)
{
    auto size = GetOpenSuccessProtocolSize();
    auto buffer = PrepareBufferAndPackHead(size, Protocol::OpenSuccess, id);
    auto buf = buffer->buf + GetProtocolHeadSize();

    *reinterpret_cast<unsigned int *>(buf) = htonl(ip);
    buf += sizeof(ip);

    *reinterpret_cast<unsigned short *>(buf) = htons(port);
    return buffer;
}

inline std::unique_ptr<snet::Buffer> PackClose(unsigned long long id)
{
    auto size = GetCloseProtocolSize();
    return PrepareBufferAndPackHead(size, Protocol::Close, id);
}

inline std::unique_ptr<snet::Buffer> PackShutdownWrite(unsigned long long id)
{
    auto size = GetShutdownWriteProtocolSize();
    return PrepareBufferAndPackHead(size, Protocol::ShutdownWrite, id);
}

inline std::unique_ptr<snet::Buffer> PackData(unsigned long long id,
                                              const snet::Buffer &buffer)
{
    auto size = GetDataProtocolSize(buffer.size);
    auto data = PrepareBufferAndPackHead(size, Protocol::Data, id);
    auto buf = data->buf + GetProtocolHeadSize();

    memcpy(buf, buffer.buf, buffer.size);
    return data;
}

inline Protocol UnpackProtocolType(snet::Buffer &buffer)
{
    if (buffer.pos >= buffer.size)
        return Protocol::Unknown;

    unsigned char c = *(buffer.buf + buffer.pos);
    buffer.pos += 1;

    if (c == static_cast<unsigned char>(Protocol::Open))
        return Protocol::Open;
    else if (c == static_cast<unsigned char>(Protocol::OpenSuccess))
        return Protocol::OpenSuccess;
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
    if (buffer.pos + GetOpenProtocolSize("") - 1 >= buffer.size)
        return false;

    *id = UnpackId(buffer);

    auto len = buffer.size - buffer.pos - sizeof(*port);
    host->assign(buffer.buf + buffer.pos, buffer.buf + buffer.pos + len);
    buffer.pos += len;

    *port = ntohs(*reinterpret_cast<unsigned short *>(buffer.buf + buffer.pos));
    buffer.pos += sizeof(*port);

    return true;
}

inline bool UnpackOpenSuccess(snet::Buffer &buffer, unsigned long long *id,
                              unsigned int *ip, unsigned short *port)
{
    if (buffer.pos + GetOpenSuccessProtocolSize() - 1 > buffer.size)
        return false;

    *id = UnpackId(buffer);

    *ip = *reinterpret_cast<unsigned int *>(buffer.buf + buffer.pos);
    buffer.pos += sizeof(*ip);

    *port = *reinterpret_cast<unsigned short *>(buffer.buf + buffer.pos);
    buffer.pos += sizeof(*port);

    return true;
}

inline bool UnpackClose(snet::Buffer &buffer, unsigned long long *id)
{
    if (buffer.pos + GetCloseProtocolSize() - 1 > buffer.size)
        return false;

    *id = UnpackId(buffer);
    return true;
}

inline bool UnpackShutdownWrite(snet::Buffer &buffer, unsigned long long *id)
{
    if (buffer.pos + GetShutdownWriteProtocolSize() - 1 > buffer.size)
        return false;

    *id = UnpackId(buffer);
    return true;
}

inline bool UnpackData(snet::Buffer &buffer, unsigned long long *id)
{
    if (buffer.pos + GetDataProtocolSize(0) - 1 >= buffer.size)
        return false;

    *id = UnpackId(buffer);
    return true;
}

} // namespace stunnel

#endif // STUNNEL_H
