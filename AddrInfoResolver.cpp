#include "AddrInfoResolver.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <atomic>
#include <thread>

namespace snet
{

struct AddrInfoResolver::Request
{
    const std::string host;
    OnResolve on_resolve;
    struct addrinfo *result;

    Request(const std::string &h, const OnResolve &onr)
        : host(h),
          on_resolve(onr),
          result(nullptr)
    {
    }

    Request(const Request &) = delete;
    void operator = (const Request &) = delete;

    ~Request()
    {
        if (result)
            freeaddrinfo(result);
    }
};

class AddrInfoResolver::Resolver final
{
public:
    explicit Resolver(snet::MessageQueue<Request *> *resolved)
        : resolved_(resolved),
          counter_(0),
          thread_(&Resolver::ResolveFunc, this)
    {
    }

    ~Resolver()
    {
        Resolve(nullptr);
        thread_.join();
    }

    Resolver(const Resolver &) = delete;
    void operator = (const Resolver &) = delete;

    void Resolve(Request *request)
    {
        if (request)
            ++counter_;
        requests_.Send(request);
    }

    int GetRequestCount() const
    {
        return counter_;
    }

private:
    void ResolveFunc()
    {
        while (true)
        {
            auto request = requests_.Recv();
            if (!request)
                break;

            ResolveRequest(request);
        }
    }

    void ResolveRequest(Request *request)
    {
        getaddrinfo(request->host.c_str(), nullptr, nullptr, &request->result);
        resolved_->Send(request);
        --counter_;
    }

    snet::MessageQueue<Request *> requests_;
    snet::MessageQueue<Request *> *resolved_;
    std::atomic<int> counter_;
    std::thread thread_;
};

AddrInfoResolver::AddrInfoResolver(std::size_t resolver_num)
{
    for (std::size_t i = 0; i < resolver_num; ++i)
    {
        resolvers_.push_back(
            std::unique_ptr<Resolver>(new Resolver(&resolved_)));
    }
}

AddrInfoResolver::~AddrInfoResolver()
{
}

const AddrInfoResolver::Request * AddrInfoResolver::AsyncResolve(
    const std::string &host, const OnResolve &on_resolve)
{
    auto req = new Request(host, on_resolve);
    std::unique_ptr<Request> request(req);

    requests_.push_back(std::move(request));

    auto index = FindResolver();
    resolvers_[index]->Resolve(req);

    return req;
}

void AddrInfoResolver::CancelRequest(const Request *request)
{
    for (auto it = requests_.begin(); it != requests_.end(); ++it)
    {
        if (it->get() == request)
        {
            (*it)->on_resolve = nullptr;
            return ;
        }
    }
}

void AddrInfoResolver::HandleLoop()
{
    if (!requests_.empty())
    {
        while (true)
        {
            auto resolved = resolved_.TryRecv();
            if (!resolved)
                break;

            auto request = *resolved;
            HandleResolve(*request);
            RemoveRequest(request);
        }
    }
}

void AddrInfoResolver::HandleStop()
{
}

std::size_t AddrInfoResolver::FindResolver() const
{
    std::size_t index = 0;
    auto min = resolvers_[index]->GetRequestCount();

    auto size = resolvers_.size();
    for (std::size_t i = 1; i < size; ++i)
    {
        auto count = resolvers_[i]->GetRequestCount();
        if (count < min)
        {
            min = count;
            index = i;
        }
    }

    return index;
}

void AddrInfoResolver::HandleResolve(const Request &request)
{
    if (!request.on_resolve)
        return ;

    SockAddrs addrs;

    for (auto res = request.result; res; res = res->ai_next)
    {
        if (res->ai_family == AF_INET && res->ai_socktype == SOCK_STREAM)
            addrs.push_back(res->ai_addr);
    }

    request.on_resolve(addrs);
}

void AddrInfoResolver::RemoveRequest(const Request *request)
{
    for (auto it = requests_.begin(); it != requests_.end(); ++it)
    {
        if (it->get() == request)
        {
            requests_.erase(it);
            return ;
        }
    }
}

} // namespace snet
