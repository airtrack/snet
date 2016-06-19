#include "Cipher.h"
#include <string.h>
#include <stdlib.h>

namespace cipher
{

void IVec::RandomReset()
{
    auto *i = reinterpret_cast<unsigned int *>(ivec);
    *i++ = random();
    *i = random();
}

Cryptor::Cryptor(const char *key, std::size_t len, Type type)
    : type_(type == Type::Encrypt ? BF_ENCRYPT : BF_DECRYPT),
      num_(0)
{
    memset(ivec_.ivec, 0, sizeof(ivec_.ivec));
    BF_set_key(&key_, len, reinterpret_cast<const unsigned char *>(key));
}

void Cryptor::SetIVec(const IVec &ivec)
{
    ivec_ = ivec;
    num_ = 0;
}

std::unique_ptr<snet::Buffer> Cryptor::Crypt(
    std::unique_ptr<snet::Buffer> buffer)
{
    auto size = buffer->size - buffer->pos;
    if (size == 0)
        return std::unique_ptr<snet::Buffer>(new snet::Buffer(0, 0));

    std::unique_ptr<snet::Buffer> data(
        new snet::Buffer(new char[size], size, snet::OpDeleter));

    auto in = reinterpret_cast<unsigned char *>(buffer->buf + buffer->pos);
    auto out = reinterpret_cast<unsigned char *>(data->buf);
    BF_cfb64_encrypt(in, out, size, &key_, ivec_.ivec, &num_, type_);

    return data;
}

} // namespace cipher
