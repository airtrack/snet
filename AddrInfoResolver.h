#ifndef ADDR_INFO_RESOLVER_H
#define ADDR_INFO_RESOLVER_H

#include "EventLoop.h"
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

    AddrInfoResolver();
    ~AddrInfoResolver();

    AddrInfoResolver(const AddrInfoResolver &) = delete;
    void operator = (const AddrInfoResolver &) = delete;

    const Request * AsyncResolve(const std::string &host,
                                 const OnResolve &on_resolve);
    void CancelRequest(const Request *request);

    virtual void HandleLoop() override;
    virtual void HandleStop() override;

private:
    void CheckResult();
    void ResolveSuccess(const Request &request);
    void ResolveFail(const Request &request);

    std::vector<std::unique_ptr<Request>> requests_;
};

} // namespace snet

#endif // ADDR_INFO_RESOLVER_H
