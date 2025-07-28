/*
 * @Author: star-cs
 * @Date: 2025-07-28 10:50:21
 * @LastEditTime: 2025-07-28 11:44:04
 * @FilePath: /TMMS-SERVER/tests/mmedia/test_rtmpserver.cpp
 * @Description:
 */
#include "base/log/log.h"
#include "mmedia/rtmp/rtmp_server.h"
#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"

using namespace tmms::net;
using namespace tmms::base;
using namespace tmms::mm;

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
        InetAddress listen("0.0.0.0:1935");
        RtmpServer  server(loop, listen);

        server.Start();
        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}