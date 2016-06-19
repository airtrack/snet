#ifndef CIPHER_H
#define CIPHER_H

#include "Buffer.h"
#include <openssl/blowfish.h>
#include <memory>

namespace cipher
{

struct IVec
{
    unsigned char ivec[8];

    void RandomReset();
};

class Cryptor
{
public:
    enum class Type
    {
        Encrypt,
        Decrypt
    };

    Cryptor(const char *key, std::size_t len, Type type);

    Cryptor(const Cryptor &) = delete;
    void operator = (const Cryptor &) = delete;

    void SetIVec(const IVec &ivec);
    std::unique_ptr<snet::Buffer> Crypt(std::unique_ptr<snet::Buffer> buffer);

private:
    int type_;
    int num_;
    IVec ivec_;
    BF_KEY key_;
};

class Encryptor final : private Cryptor
{
public:
    Encryptor(const char *key, std::size_t len)
        : Cryptor(key, len, Type::Encrypt)
    {
    }

    using Cryptor::SetIVec;
    std::unique_ptr<snet::Buffer> Encrypt(std::unique_ptr<snet::Buffer> buffer)
    {
        return Crypt(std::move(buffer));
    }
};

class Decryptor final : private Cryptor
{
public:
    Decryptor(const char *key, std::size_t len)
        : Cryptor(key, len, Type::Decrypt)
    {
    }

    using Cryptor::SetIVec;
    std::unique_ptr<snet::Buffer> Decrypt(std::unique_ptr<snet::Buffer> buffer)
    {
        return Crypt(std::move(buffer));
    }
};

} // namespace cipher

#endif // CIPHER_H
