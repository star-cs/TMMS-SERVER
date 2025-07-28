/*
 * @Author: star-cs
 * @Date: 2025-07-27 20:56:58
 * @LastEditTime: 2025-07-27 21:02:33
 * @FilePath: /TMMS-SERVER/tmms/mmedia/rtmp/rtmp_server.h
 * @Description:
 */
#pragma once

#include "mmedia/rtmp/rtmp_handler.h"
#include "network/base/inetaddress.h"
#include "network/net/eventloop.h"
#include "network/net/tcpserver.h"
namespace tmms::mm
{
using namespace tmms::net;

class RtmpServer : public TcpServer
{
public:
    RtmpServer(EventLoop* loop, const InetAddress& local, RtmpHandler* hander = nullptr);
    ~RtmpServer();

    void Start() override;
    void Stop() override;
    void OnNewConnection(const TcpConnectionPtr& conn);
    void OnConnectionDestroy(const TcpConnectionPtr& conn);
    void OnMessage(const TcpConnectionPtr& conn, MsgBuffer& buf);
    void OnWriteComplete(const ConnectionPtr& conn);
    void OnActive(const ConnectionPtr& conn);

    RtmpHandler* rtmp_handler_{nullptr};
};

} // namespace tmms::mm