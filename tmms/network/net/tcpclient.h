/*
 * @Author: star-cs
 * @Date: 2025-07-24 14:00:28
 * @LastEditTime: 2025-07-25 22:42:20
 * @FilePath: /TMMS-SERVER/tmms/network/net/tcpclient.h
 * @Description:
 */
#pragma once

#include "network/net/tcpconnection.h"
#include <algorithm>

namespace tmms::net
{
enum
{
    kTcpConStatusInit         = 0,
    kTcpConStatusConnecting   = 1,
    kTcpConStatusConnected    = 2,
    kTcpConStatusDisConnected = 3,
};

using ConnectionCallback =
    std::function<void(const TcpConnectionPtr& con, bool)>; // 连接的是哪个tcpconnection，是否连接

class TcpClient : public TcpConnection
{
public:
    TcpClient(EventLoop* loop, const InetAddress& server); // loop要有，类是继承Event的，加入监听
    virtual ~TcpClient();

public:
    void SetConnectCallback(const ConnectionCallback& cb) { connected_cb_ = cb; }
    void SetConnectCallback(ConnectionCallback&& cb) { connected_cb_ = std::move(cb); }
    void Connect();

    void OnRead() override;
    void OnWrite() override;
    void OnClose() override;

    void Send(std::list<BufferNode::ptr>& list);
    void Send(const char* buf, size_t size);

private:
    void ConnectInLoop();
    void UpdateConnectionStatus();
    bool CheckError();

    InetAddress        server_addr_; // 服务端地址
    int32_t            status_{kTcpConStatusInit};
    ConnectionCallback connected_cb_; // 连接回调
};
} // namespace tmms::net