#pragma once

#include "network/base/inetaddress.h"
#include "network/base/socketopt.h"
#include "network/net/event.h"

#include <functional>
#include <memory>
namespace tmms::net
{

using AcceptCallback = std::function<void(int sock, const InetAddress& addr)>;

class Acceptor : public Event
{
public:
    using ptr = std::shared_ptr<Acceptor>;
    Acceptor(EventLoop* loop, const InetAddress& addr);
    ~Acceptor();

    void SetAcceptCallback(const AcceptCallback&& cb) { accept_cb_ = cb; }
    void SetAcceptCallback(const AcceptCallback& cb) { accept_cb_ = cb; }

    void Start();
    void Stop();
    void OnRead() override;
    void OnClose() override;
    void OnError(const std::string& msg) override;

private:
    void           Open();
    InetAddress    addr_;
    AcceptCallback accept_cb_;
    SocketOpt::ptr socket_opt_;
};

} // namespace tmms::net