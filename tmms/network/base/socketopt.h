#pragma once
#include "network/base/inetaddress.h"

namespace tmms::net
{
class SocketOpt
{
public:
    SocketOpt(int sock, bool v6 = false);
    ~SocketOpt() = default;

    /// 服务端
    static int CreateNonblockingTcpSocket(int family);
    static int CreateNonblockingUdpSocket(int family);

    int BindAddress(const InetAdress& localaddr);
    int Listen();
    int Accept(InetAdress* peeraddr);

    /// 客户端
    int Connect(const InetAdress& addr);

public:
    InetAdress::ptr GetLocalAddr(); // 本地地址
    InetAdress::ptr GetPeerAddr();  // 远端地址
    void            SetTcpNoDelay(bool on);
    void            SetReuseAddr(bool on);
    void            SetReusePort(bool on);
    void            SetNonBlocking(bool on);

private:
    int  sock_{-1};
    bool is_v6_{false};
};
} // namespace tmms::net