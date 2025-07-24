
#include "base/log/log.h"
#include "network/base/inetaddress.h"
#include "network/net/acceptor.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "network/net/tcpconnection.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace tmms::base;
using namespace tmms::net;

EventLoopThread eventloop_thread;
std::thread     th;

// ab -c 1 -n 1 "http://127.0.1.1:34444/" 测试

const char* http_response = "HTTP/1.0 200 OK\r\nServer: tmms\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";
void        test()
{
    std::vector<TcpConnectionPtr> list;
    eventloop_thread.Run();
    EventLoop*    loop = eventloop_thread.Loop();
    InetAddress   server("0.0.0.0:34444");
    Acceptor::ptr acceptor = std::make_shared<Acceptor>(loop, server);
    acceptor->SetAcceptCallback(
        [&loop, &server, &list](int fd, const InetAddress& addr)
        {
            CORE_DEBUG("host : {}", addr.ToIpPort());
            TcpConnectionPtr connection = std::make_shared<TcpConnection>(loop, fd, server, addr);
            connection->SetRecvMsgCallback(
                [](const TcpConnectionPtr& conn, MsgBuffer& buf)
                {
                    CORE_DEBUG("recv msg{}", buf.Peek());
                    buf.RetrieveAll();
                    conn->Send(http_response, strlen(http_response));
                });
            connection->SetWriteCompleteCallback(
                [&loop](const TcpConnectionPtr& conn)
                {
                    CORE_DEBUG("write complete host : {}", conn->PeerAddr().ToIpPort());
                    loop->DelEvent(conn);
                    conn->ForceClose();
                });
            connection->EnableCheckIdleTimeout(3);
            list.push_back(connection);
            loop->AddEvent(connection);
        });

    acceptor->Start();

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    Log::init(true);
    test();
    return 0;
}