#include "base/log/log.h"
#include "network/base/inetaddress.h"
#include "network/base/socketopt.h"
#include <iostream>
#include <sys/socket.h>

using namespace tmms::net;
using namespace tmms::base;

/// 客户端
// nc -l 34444模拟监听
void TestClient()
{
    int sock = SocketOpt::CreateNonblockingTcpSocket(AF_INET);
    if (sock < 0)
    {
        CORE_DEBUG("Socket failed.sock {} errno: {}", sock, errno);
        return;
    }

    InetAdress server("0.0.0.0:34444");
    SocketOpt  opt(sock);
    opt.SetNonBlocking(false);

    auto ret = opt.Connect(server);
    CORE_DEBUG(
        "connect ret : {} errno : {} loacl: {} perr:{}",
        ret,
        errno,
        opt.GetLocalAddr()->ToIpPort(),
        opt.GetPeerAddr()->ToIpPort());
}

// 模拟客户端
// sudo apt install apache2-utils
// ab -c 1 -n 1 "http://127.0.0.1:34444/"
void TestServer()
{
    // 创建一个socket
    int sock = SocketOpt::CreateNonblockingTcpSocket(AF_INET);
    if (sock < 0)
    {
        CORE_DEBUG("Socket failed.sock {} errno: {}", sock, errno);
        return;
    }

    // 服务器地址
    InetAdress server("0.0.0.0:34444");
    SocketOpt  opt(sock);
    opt.SetNonBlocking(false);

    opt.BindAddress(server);
    opt.Listen();
    InetAdress addr;
    auto       ns = opt.Accept(&addr);

    CORE_DEBUG("accept ret : {}    errno :{} addr: {} ", ns, errno, addr.ToIpPort());
}

int main()
{
    Log::init(true);
    // TestClient();
    TestServer();
    return 0;
}