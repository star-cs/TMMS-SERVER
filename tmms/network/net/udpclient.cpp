#include "network/net/udpclient.h"
#include "network/base/inetaddress.h"
#include "network/base/socketopt.h"
#include "network/net/eventloop.h"
#include "network/net/udpsocket.h"
#include <memory>
#include <netinet/in.h>

namespace tmms::net
{
UdpClient::UdpClient(EventLoop* loop, const InetAddress& server)
    : UdpSocket(loop, -1, InetAddress(), server),
      server_addr_(server)
{
}

UdpClient::~UdpClient()
{
}

/// @brief 对外调用
void UdpClient::Connect()
{
    loop_->RunInLoop([this]() { ConnectInLoop(); });
}

/// @brief 创建udpsocket，进行connect，添加监听事件
void UdpClient::ConnectInLoop()
{
    loop_->AssertInLoopThread();
    // 在这里正式创建 udp socket
    fd_ = SocketOpt::CreateNonblockingUdpSocket(AF_INET);
    if (fd_ < 0)
    {
        OnClose();
        return;
    }
    connected_ = true;
    loop_->AddEvent(std::dynamic_pointer_cast<UdpClient>(shared_from_this()));
    SocketOpt opt(fd_);
    opt.Connect(server_addr_);
    server_addr_.GetSockAddr((struct sockaddr*)&sock_addr_);

    if (connected_cb_)
    {
        connected_cb_(std::dynamic_pointer_cast<UdpSocket>(shared_from_this()), true);
    }
}

void UdpClient::Send(std::list<UdpBufferNodePtr>& list)
{
    UdpSocket::Send(list);
}
void UdpClient::Send(const char* buf, size_t size)
{
    UdpSocket::Send(buf, size, (struct sockaddr*)&sock_addr_, sock_len_);
}

void UdpClient::OnClose()
{
    if (connected_)
    {
        loop_->DelEvent(std::dynamic_pointer_cast<UdpClient>(shared_from_this()));
        connected_ = false;
        UdpSocket::OnClose();
    }
}
} // namespace tmms::net