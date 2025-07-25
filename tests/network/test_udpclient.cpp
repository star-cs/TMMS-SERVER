/*
 * @Author: star-cs
 * @Date: 2025-07-25 22:11:24
 * @LastEditTime: 2025-07-25 22:12:36
 * @FilePath: /TMMS-SERVER/tests/network/test_udpclient.cpp
 * @Description:
 */
#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "network/net/udpclient.h"
#include <iostream>
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
        InetAddress server("127.0.0.1:34444");

        std::shared_ptr<UdpClient> client = std::make_shared<UdpClient>(loop, server);

        client->SetRecvMsgCallback(
            [](const InetAddress& addr, MsgBuffer& buf)
            {
                std::cout << "host : " << addr.ToIpPort() << " \nmsg : " << buf.Peek() << std::endl;
                buf.RetrieveAll();
            });

        client->SetCloseCallback(
            [](const UdpSocketPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " closed" << std::endl;
                }
            });

        client->SetWriteCompleteCallback(
            [](const UdpSocketPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " write complete" << std::endl;
                }
            });

        client->SetConnectedCallback(
            [&client](const UdpSocketPtr& con, bool connected)
            {
                if (connected)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " connected." << std::endl;
                    client->Send("1111", strlen("1111"));
                }
            });
            
        client->Connect();

        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}