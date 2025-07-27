/*
 * @Author: star-cs
 * @Date: 2025-07-27 19:09:14
 * @LastEditTime: 2025-07-27 19:14:48
 * @FilePath: /TMMS-SERVER/tests/mmedia/test_handshakeclient.cpp
 * @Description:
 */

 #include "mmedia/rtmp/rtmp_hand_shake.h"
 #include "network/base/inetaddress.h"
 #include "network/net/eventloopthread.h"
 #include "network/net/tcpconnection.h"
 #include "network/net/tcpserver.h"
 #include <iostream>
 #include <memory>
 
 using namespace tmms::net;
 using namespace tmms::mm;
 
 EventLoopThread eventloop_thread;
 
 std::thread th;
 
 const char* http_response = "HTTP/1.0 200 OK\r\nServer: tmms\r\nContent-Type: text/html\r\nContent-Length: 0\r\n\r\n";
 
 int main()
 {
     eventloop_thread.Run();
 
     EventLoop* loop = eventloop_thread.Loop();
 
     if (loop)
     {
         InetAddress listen("0.0.0.0:1935");
         TcpServer   server(loop, listen);
         server.SetMessageCallback(
             [](const TcpConnectionPtr& con, MsgBuffer& buf)
             {
                 RtmpHandShake::ptr shake = con->GetContext<RtmpHandShake>(kNormalContext);
                 // 每次收到消息就进行回调
                 shake->HandShake(buf);
             });
 
         // 设置有新连接的时候，设置上下文到conn上，设置上下文解析完成之后的回调
         server.SetNewConnectionCallback(
             [](const TcpConnection::ptr& con)
             {
                 RtmpHandShake::ptr shake = std::make_shared<RtmpHandShake>(con, false);
                 con->SetContext(kNormalContext, shake);
 
                 // 服务器等待 C0C1
                 shake->Start();
 
                 con->SetWriteCompleteCallback(
                     [](const TcpConnection::ptr& con)
                     {
                         std::cout << "write complete host： " << con->PeerAddr().ToIpPort() << std::endl;
                         RtmpHandShake::ptr shake = con->GetContext<RtmpHandShake>(kNormalContext);
 
                         // 驱动状态机，每次写完都会启动这个回调
                         shake->WriteComplete();
                     });
             });
 
         server.Start();
 
         while (1)
         {
             std::this_thread::sleep_for(std::chrono::seconds(1));
         }
     }
 }