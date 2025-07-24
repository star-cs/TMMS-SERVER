#pragma once
#include "network/base/inetaddress.h"
#include <memory>

namespace tmms::net
{
class SocketOpt
{
public:
    using ptr = std::shared_ptr<SocketOpt>;
    
    SocketOpt(int sock, bool v6 = false);
    ~SocketOpt() = default;

    /// 服务端
    static int CreateNonblockingTcpSocket(int family);
    static int CreateNonblockingUdpSocket(int family);

    int BindAddress(const InetAddress& localaddr);
    int Listen();
    int Accept(InetAddress* peeraddr);

    /// 客户端
    int Connect(const InetAddress& addr);

public:
    InetAddress::ptr GetLocalAddr(); // 本地地址
    InetAddress::ptr GetPeerAddr();  // 远端地址
    void             SetTcpNoDelay(bool on);
    void             SetReuseAddr(bool on);
    void             SetReusePort(bool on);
    void             SetNonBlocking(bool on);

private:
    int  sock_{-1};
    bool is_v6_{false};
};
} // namespace tmms::net