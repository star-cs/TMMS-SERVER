#include "network/net/udpserver.h"
#include "network/base/inetaddress.h"
#include "network/base/socketopt.h"
#include "network/net/eventloop.h"
#include "network/net/udpsocket.h"

using namespace tmms::net;

UdpServer::UdpServer(EventLoop* loop, InetAddress& server) : UdpSocket(loop, -1, server, InetAddress()), server_(server)
{
}
UdpServer::~UdpServer()
{
    Stop();
}

/// 对外调用
void UdpServer::Start()
{
    loop_->RunInLoop([this]() { Open(); });
}

/// 对外调用
void UdpServer::Stop()
{
    loop_->RunInLoop(
        [this]()
        {
            loop_->DelEvent(std::dynamic_pointer_cast<UdpServer>(shared_from_this()));
            OnClose(); // 关闭套接字
        });
}

/// @brief  进行udp的socket创建
void UdpServer::Open()
{
    loop_->AssertInLoopThread();
    fd_ = SocketOpt::CreateNonblockingUdpSocket(AF_INET);
    // 1. 申请失败，关闭
    if (fd_ < 0)
    {
        OnClose();
        return;
    }
    // 2. 申请成功，加入事件循环，绑定，加入EPOLLIN
    loop_->AddEvent(std::dynamic_pointer_cast<UdpServer>(shared_from_this()));
    SocketOpt opt(fd_);
    opt.BindAddress(server_);
}
