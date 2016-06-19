#ifndef ADDR_INFO_RESOLVER_H
#define ADDR_INFO_RESOLVER_H

#include "EventLoop.h"
#include "MessageQueue.h"
#include <arpa/inet.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace snet
{

class AddrInfoResolver final : public LoopHandler
{
public:
    struct Request;
    using SockAddrs = std::vector<const struct sockaddr *>;
    using OnResolve = std::function<void (const SockAddrs &)>;

    explicit AddrInfoResolver(std::size_t resolver_num);
    ~AddrInfoResolver();

    AddrInfoResolver(const AddrInfoResolver &) = delete;
    void operator = (const AddrInfoResolver &) = delete;

    const Request * AsyncResolve(const std::string &host,
                                 const OnResolve &on_resolve);
    void CancelRequest(const Request *request);

    virtual void HandleLoop() override;
    virtual void HandleStop() override;

private:
    class Resolver;

    std::size_t FindResolver() const;
    void HandleResolve(const Request &request);
    void RemoveRequest(const Request *request);

    snet::MessageQueue<Request *> resolved_;
    std::vector<std::unique_ptr<Request>> requests_;
    std::vector<std::unique_ptr<Resolver>> resolvers_;
};

} // namespace snet

#endif // ADDR_INFO_RESOLVER_H
