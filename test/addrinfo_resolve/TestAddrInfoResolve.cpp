#include "EventLoop.h"
#include "AddrInfoResolver.h"
#include <stdio.h>
#include <string>

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s HOST ...\n", argv[0]);
        return 1;
    }

    auto event_loop = snet::CreateEventLoop();
    snet::AddrInfoResolver addrinfo_resolver;

    auto cancel = addrinfo_resolver.AsyncResolve(
        "cancel.com", [] (const snet::AddrInfoResolver::SockAddrs &) { });

    auto num = argc - 1;
    for (auto i = 1; i < argc; ++i)
    {
        std::string host = argv[i];
        addrinfo_resolver.AsyncResolve(
            host, [&, host] (const snet::AddrInfoResolver::SockAddrs &addrs) {
                fprintf(stderr, "%s: ", host.c_str());

                for (auto addr : addrs)
                {
                    if (addr->sa_family == AF_INET)
                    {
                        char buf[INET_ADDRSTRLEN] = { 0 };
                        auto inet = reinterpret_cast<const sockaddr_in *>(addr);
                        inet_ntop(AF_INET, &inet->sin_addr, buf, sizeof(buf));
                        fprintf(stderr, "%s ", buf);
                    }
                }

                fprintf(stderr, "\n");

                if (--num == 0)
                    event_loop->Stop();
            });
    }

    addrinfo_resolver.CancelRequest(cancel);

    event_loop->AddLoopHandler(&addrinfo_resolver);
    event_loop->Loop();

    return 0;
}
