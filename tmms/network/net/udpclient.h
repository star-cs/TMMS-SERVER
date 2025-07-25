/*
 * @Author: star-cs
 * @Date: 2025-07-25 21:57:42
 * @LastEditTime: 2025-07-25 21:57:58
 * @FilePath: /TMMS-SERVER/tmms/network/net/udpclient.h
 * @Description:
 */
#pragma once

#include "network/net/udpsocket.h"
#include <functional>
namespace tmms::net
{
using ConnectedCallback = std::function<void(const UdpSocketPtr&, bool)>;
class UdpClient : public UdpSocket
{
public:
    UdpClient(EventLoop* loop, const InetAddress& server);
    virtual ~UdpClient();

public:
    void Connect();
    void SetConnectedCallback(const ConnectedCallback& cb) { connected_cb_ = cb; }
    void SetConnectedCallback(ConnectedCallback&& cb) { connected_cb_ = std::move(cb); }
    void ConnectInLoop();
    void Send(std::list<UdpBufferNodePtr>& list);
    void Send(const char* buf, size_t size);
    void OnClose() override;

private:
    bool                connected_{false};
    InetAddress         server_addr_; // 服务端地址
    ConnectedCallback   connected_cb_;
    struct sockaddr_in6 sock_addr_;     // 服务端的socket地址结构
    socklen_t           sock_len_{sizeof(struct sockaddr_in6)};
};
} // namespace tmms::net