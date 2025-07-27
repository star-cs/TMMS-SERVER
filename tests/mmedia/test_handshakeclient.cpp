/*
 * @Author: star-cs
 * @Date: 2025-07-27 19:09:14
 * @LastEditTime: 2025-07-27 19:18:45
 * @FilePath: /TMMS-SERVER/tests/mmedia/test_handshakeclient.cpp
 * @Description:
 */

#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "network/base/inetaddress.h"
#include "network/net/eventloopthread.h"
#include "network/net/tcpclient.h"
#include "network/net/tcpconnection.h"
#include <iostream>
#include <memory>

using namespace tmms::net;
using namespace tmms::mm;

EventLoopThread eventloop_thread;

std::thread th;

const char* http_request  = "GET / HTTP/1.0\r\nHost: 10.101.128.69\r\nAccept: */*\r\nContent-Length: 0\r\n\r\n";
const char* http_response = "HTTP/1.0 200 OK\r\nServer: tmms\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";

int main()
{
    eventloop_thread.Run();

    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        InetAddress server("127.0.0.1:1935");

        std::shared_ptr<TcpClient> client = std::make_shared<TcpClient>(loop, server);

        client->SetRecvMsgCallback(
            [](const TcpConnectionPtr& con, MsgBuffer& buf)
            {
                RtmpHandShake::ptr shake = con->GetContext<RtmpHandShake>(kNormalContext);
                // 每次收到消息就进行回调
                shake->HandShake(buf);
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
                    RtmpHandShake::ptr shake = con->GetContext<RtmpHandShake>(kNormalContext);
                    shake->WriteComplete();
                }
            });
        client->SetConnectCallback(
            [](const TcpConnectionPtr& con, bool connected)
            {
                if (connected)
                {
                    RtmpHandShake::ptr shake = std::make_shared<RtmpHandShake>(con, true);
                    con->SetContext(kNormalContext, shake);
                    // 开始客户端发送C0C1
                    shake->Start();
                }
            });
            
        client->Connect();

        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}