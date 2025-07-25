/*
 * @Author: star-cs
 * @Date: 2025-07-25 22:26:43
 * @LastEditTime: 2025-07-25 22:28:29
 * @FilePath: /TMMS-SERVER/tests/network/test_udpserver.cpp
 * @Description:
 */

#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "network/net/udpserver.h"
#include <iostream>

#include <thread>
using namespace tmms::net;
using namespace tmms::base;

EventLoopThread eventloop_thread;
std::thread     th;

int main()
{
    eventloop_thread.Run();

    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        InetAddress                server_add("0.0.0.:34444");
        std::shared_ptr<UdpServer> server = std::make_shared<UdpServer>(loop, server_add);

        // 测试OnRead，收到什么发回什么,echo服务器
        server->SetRecvMsgCallback(
            [&server](const InetAddress& addr, MsgBuffer& buf)
            {
                std::cout << "host : " << addr.ToIpPort() << " \nmsg : " << buf.Peek() << std::endl;

                struct sockaddr_in6 saddr;
                socklen_t           len = sizeof(saddr);
                addr.GetSockAddr((struct sockaddr*)&saddr);
                server->Send(buf.Peek(), buf.ReadableBytes(), (struct sockaddr*)&saddr, len);

                buf.RetrieveAll();
            });

        server->SetCloseCallback(
            [](const UdpSocketPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " closed" << std::endl;
                }
            });

        server->SetWriteCompleteCallback(
            [](const UdpSocketPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " write complete" << std::endl;
                }
            });

        server->Start();

        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    return 0;
}
