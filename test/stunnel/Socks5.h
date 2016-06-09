#ifndef SOCKS5_H
#define SOCKS5_H

#include "Acceptor.h"
#include "Buffer.h"
#include "Connection.h"
#include <functional>
#include <string>

namespace socks5
{

class Connection final
{
public:
    enum class State
    {
        SelectingMethod,
        GettingConnectAddress,
        Connecting,
        Running,
        Closed
    };

    using OnErrorClose = std::function<void ()>;
    using OnConnectAddress = std::function<void (std::string, unsigned short)>;
    using DataHandler = std::function<void (std::unique_ptr<snet::Buffer>)>;

    explicit Connection(std::unique_ptr<snet::Connection> connection);

    Connection(const Connection &) = delete;
    void operator = (const Connection &) = delete;

    void SetOnClose(const OnErrorClose &on_error_close);
    void SetOnConnectAddress(const OnConnectAddress &oca);
    void SetDataHandler(const DataHandler &data_handler);

    void ReplyConnectSuccess(unsigned int ip, unsigned short port);
    void Send(std::unique_ptr<snet::Buffer> data);
    void Close();

private:
    void ReplyConnectResult(unsigned int ip, unsigned short port);
    void CloseConnection();
    void HandleError();
    void HandleRecv();
    void SelectMethod();
    void ReplyMethod(unsigned char method);
    void GetConnectAddress();
    void RecvData();

    static const std::size_t kMaxSelectMethodSize = 257;
    static const std::size_t kReplyMethodSize = 2;
    static const std::size_t kGetConnectAddressSize = 262;
    static const std::size_t kReplySize = 10;
    static const std::size_t kBufferSize = 2048;

    State state_;

    OnErrorClose on_error_close_;
    OnConnectAddress on_connect_address_;
    DataHandler data_handler_;

    std::unique_ptr<snet::Buffer> buffer_;
    std::unique_ptr<snet::Connection> connection_;
};

class Server final
{
public:
    using OnNewConnection = std::function<void (std::unique_ptr<Connection>)>;

    Server(const std::string &ip, unsigned short port, snet::EventLoop *loop);

    Server(const Server &) = delete;
    void operator = (const Server &) = delete;

    bool IsListenOk() const;
    void SetOnNewConnection(const OnNewConnection &onc);
    void DisableAccept();
    void EnableAccept();

private:
    void HandleNewConnection(std::unique_ptr<snet::Connection> connection);

    bool enable_accept_;
    OnNewConnection onc_;
    snet::Acceptor acceptor_;
};

} // namespace socks5

#endif // SOCKS5_H
