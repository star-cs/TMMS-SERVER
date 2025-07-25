/*
 * @Author: heart1128 1020273485@qq.com
 * @Date: 2024-06-05 16:42:13
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-07-25 22:27:44
 * @FilePath: /TMMS-SERVER/tests/network/test_tcpserver.cpp
 * @Description:  learn
 */
#include "base/log/log.h"
#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "network/net/tcpserver.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace tmms::net;
using namespace tmms::base;

EventLoopThread eventloop_thread;
std::thread     th;

const char* http_response = "HTTP/1.0 200 OK\r\nServer: tmms\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";

int main()
{
    Log::init(true);
    eventloop_thread.Run();
    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        InetAddress listen("0.0.0.0:34444");
        TcpServer   server(loop, listen);
        server.SetMessageCallback(
            [](const TcpConnectionPtr& con, MsgBuffer& buf)
            {
                std::cout << "host : " << con->PeerAddr().ToIpPort() << "msg : " << buf.Peek() << std::endl;
                buf.RetrieveAll();

                con->Send(http_response, strlen(http_response));
            });

        server.SetNewConnectionCallback(
            [](const TcpConnectionPtr& con)
            {
                con->SetWriteCompleteCallback(
                    [](const TcpConnectionPtr& con)
                    {
                        std::cout << "write complete host ： " << con->PeerAddr().ToIpPort() << std::endl;
                        con->ForceClose();
                    });
            });

        server.Start();

        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    return 0;
}