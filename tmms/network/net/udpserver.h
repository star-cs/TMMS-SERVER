/*
 * @Author: star-cs
 * @Date: 2025-07-25 22:18:08
 * @LastEditTime: 2025-07-25 22:18:17
 * @FilePath: /TMMS-SERVER/tmms/network/net/udpserver.h
 * @Description:
 */
#pragma once

#include "network/net/udpsocket.h"
namespace tmms::net
{
class UdpServer : public UdpSocket
{
public:
    UdpServer(EventLoop* loop, InetAddress& server);
    virtual ~UdpServer();

public:
    void Start();
    void Stop();

private:
    void Open();

    InetAddress server_;
};
} // namespace tmms::net
