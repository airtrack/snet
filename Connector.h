#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "Connection.h"
#include "EventLoop.h"
#include <string>
#include <memory>
#include <functional>

namespace snet
{

class Connector final
{
public:
    using ConnectionPtr = std::unique_ptr<Connection>;
    using OnConnected = std::function<void (ConnectionPtr)>;

    Connector(const std::string &ip, unsigned short port,
              EventLoop *loop);
    ~Connector();

    Connector(const Connector &) = delete;
    void operator = (const Connector &) = delete;

    void Connect(const OnConnected &oc);

private:
    class ConnectorEventHandler final : public EventHandler
    {
    public:
        explicit ConnectorEventHandler(Connector *connector)
            : connector_(connector)
        {
        }

        ConnectorEventHandler(const ConnectorEventHandler &) = delete;
        void operator = (const ConnectorEventHandler &) = delete;

        virtual int Fd() const override
        {
            return connector_->fd_;
        }

        virtual Event Events() const override
        {
            return Event::Write;
        }

        virtual Event EnabledEvents() const override
        {
            return Event::Write;
        }

        virtual void HandleRead() override { }

        virtual void HandleWrite() override
        {
            connector_->HandleConnect();
        }

    private:
        Connector *connector_;
    };

    void ProcessInProgress();
    void HandleConnect();
    void ConnectFailed();
    void ConnectSucceeded();

    int fd_;
    bool eh_registered_;
    std::string ip_;
    unsigned short port_;

    EventLoop *loop_;
    OnConnected oc_;
    ConnectorEventHandler eh_;
};

} // namespace snet

#endif // CONNECTOR_H
