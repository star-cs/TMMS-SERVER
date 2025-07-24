/*
 * @Author: heart1128 1020273485@qq.com
 * @Date: 2024-06-05 16:42:13
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-07-24 14:31:34
 * @FilePath: /TMMS-SERVER/tests/network/test_tcpclient.cpp
 * @Description:  learn
 */
#include "base/log/log.h"

#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "network/net/tcpclient.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace tmms::net;
using namespace tmms::base;

EventLoopThread eventloop_thread;
std::thread     th;

const char* http_request  = "GET / HTTP/1.0\r\nHost: 127.0.0.1\r\nAccept: */*\r\nContent-Length: 0\r\n\r\n";
const char* http_response = "HTTP/1.0 200 OK\r\nServer: tmms\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";

int main()
{
    Log::init(true);
    eventloop_thread.Run();
    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        InetAddress server("127.0.0.1:34444");
        // 继承了std::enable_shared_from_this<Event>必须使用智能指针
        std::shared_ptr<TcpClient> client = std::make_shared<TcpClient>(loop, server);
        client->SetRecvMsgCallback(
            [](const TcpConnectionPtr& con, MsgBuffer& buf)
            {
                std::cout << "host : " << con->PeerAddr().ToIpPort() << "msg : " << buf.Peek() << std::endl;
                buf.RetrieveAll();

                con->Send(http_response, strlen(http_response));
            });

        client->SetCloseCallback(
            [](const TcpConnectionPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " closed" << std::endl;
                }
            });

        client->SetWriteCompleteCallback(
            [](const TcpConnectionPtr& con)
            {
                if (con)
                {
                    std::cout << "host: " << con->PeerAddr().ToIpPort() << " write complete" << std::endl;
                }
            });

        client->SetConnectCallback(
            [](const TcpConnectionPtr& con, bool connected)
            {
                if (connected)
                {
                    con->Send(http_request, strlen(http_request));
                }
            });
        client->Connect();

        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    return 0;
}