#include "AddrInfoResolver.h"
#include <netdb.h>
#include <string.h>

namespace snet
{

struct AddrInfoResolver::Request
{
    std::string host;
    OnResolve on_resolve;
    struct gaicb req;

    Request(const std::string &h, const OnResolve &onr)
        : host(h),
          on_resolve(onr)
    {
        memset(&req, 0, sizeof(req));
        req.ar_name = host.c_str();
    }

    Request(const Request &) = delete;
    void operator = (const Request &) = delete;

    ~Request()
    {
        if (req.ar_result)
            freeaddrinfo(req.ar_result);
    }
};

AddrInfoResolver::AddrInfoResolver()
{
}

AddrInfoResolver::~AddrInfoResolver()
{
}

const AddrInfoResolver::Request * AddrInfoResolver::AsyncResolve(
    const std::string &host, const OnResolve &on_resolve)
{
    std::unique_ptr<Request> request(new Request(host, on_resolve));

    struct gaicb *list[] = { &request->req };
    auto ret = getaddrinfo_a(GAI_NOWAIT, list, 1, nullptr);

    if (ret)
    {
        ResolveFail(*request);
        return nullptr;
    }
    else
    {
        requests_.push_back(std::move(request));
        return requests_.back().get();
    }
}

void AddrInfoResolver::CancelRequest(const Request *request)
{
    for (auto it = requests_.begin(); it != requests_.end(); ++it)
    {
        if (it->get() == request)
        {
            gai_cancel(&(*it)->req);
            requests_.erase(it);
            return ;
        }
    }
}

void AddrInfoResolver::HandleLoop()
{
    if (!requests_.empty())
    {
        std::vector<struct gaicb *> requests;
        requests.reserve(requests_.size());

        for (const auto &req : requests_)
            requests.push_back(&req->req);

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;

        gai_suspend(&requests[0], requests.size(), &timeout);
        CheckResult();
    }
}

void AddrInfoResolver::HandleStop()
{
    gai_cancel(nullptr);
    requests_.clear();
}

void AddrInfoResolver::CheckResult()
{
    std::size_t index = 0;

    while (index < requests_.size())
    {
        auto it = requests_.begin() + index;
        auto ret = gai_error(&(*it)->req);

        if (ret == 0)
            ResolveSuccess(**it);
        else if (ret != EAI_INPROGRESS)
            ResolveFail(**it);

        if (ret == EAI_INPROGRESS)
            ++index;
        else
            requests_.erase(it);
    }
}

void AddrInfoResolver::ResolveSuccess(const Request &request)
{
    SockAddrs addrs;

    for (auto res = request.req.ar_result; res; res = res->ai_next)
    {
        if (res->ai_family == AF_INET && res->ai_socktype == SOCK_STREAM)
            addrs.push_back(res->ai_addr);
    }

    request.on_resolve(addrs);
}

void AddrInfoResolver::ResolveFail(const Request &request)
{
    SockAddrs addrs;
    request.on_resolve(addrs);
}

} // namespace snet
