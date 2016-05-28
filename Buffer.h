#ifndef BUFFER_H
#define BUFFER_H

#include <cstddef>

namespace snet
{

struct Buffer
{
    using Destruct = void (*)(Buffer *);

    char *buf;
    std::size_t size;
    std::size_t pos;
    Destruct destruct;

    Buffer(char *b, std::size_t s, Destruct d = nullptr)
        : buf(b), size(s), pos(0), destruct(d)
    {
    }

    Buffer(const Buffer &) = delete;
    void operator = (const Buffer &) = delete;

    ~Buffer()
    {
        if (destruct)
            destruct(this);
    }
};

inline void OpDeleter(Buffer *buffer)
{
    delete [] buffer->buf;
}

} // namespace snet

#endif // BUFFER_H
