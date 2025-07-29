#include "rtmp_server.h"
#include "base/log/log.h"
#include "mmedia/rtmp/rtmp_context.h"
#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "network/net/connection.h"
#include "network/net/tcpserver.h"
#include <memory>

namespace tmms::mm
{
RtmpServer::RtmpServer(EventLoop* loop, const InetAddress& local, RtmpHandler* hander)
    : TcpServer(loop, local),
      rtmp_handler_(hander)
{
}
RtmpServer::~RtmpServer()
{
}

/// @brief 全部由tcpserver处理回调，但是调用的都是rtmpserver设置的函数
void RtmpServer::Start()
{
    TcpServer::SetActiveCallback(std::bind(&RtmpServer::OnActive, this, std::placeholders::_1));
    TcpServer::SetDestroyConnectionCallback(std::bind(&RtmpServer::OnConnectionDestroy, this, std::placeholders::_1));
    TcpServer::SetNewConnectionCallback(std::bind(&RtmpServer::OnNewConnection, this, std::placeholders::_1));
    TcpServer::SetWriteCompleteCallback(std::bind(&RtmpServer::OnWriteComplete, this, std::placeholders::_1));
    TcpServer::SetMessageCallback(
        std::bind(&RtmpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    TcpServer::Start();
}

void RtmpServer::Stop()
{
    TcpServer::Stop();
}

/// @brief  新连接回调
/// @param conn
void RtmpServer::OnNewConnection(const TcpConnectionPtr& conn)
{
    // 1. 通知上层
    if (rtmp_handler_)
    {
        rtmp_handler_->OnNewConnection(conn);
    }
    // 2. 处理rtmp服务端连接
    RtmpContextPtr context = std::make_shared<RtmpContext>(conn, nullptr);
    conn->SetContext(kRtmpContext, context);
    context->StarHandShake(); // 等待C0C1
}

void RtmpServer::OnConnectionDestroy(const TcpConnectionPtr& conn)
{
    if (rtmp_handler_)
    {
        rtmp_handler_->OnConnectionDestroy(conn);
    }
    conn->ClearContext(kRtmpContext);
}

void RtmpServer::OnMessage(const TcpConnectionPtr& conn, MsgBuffer& buf)
{
    RtmpContextPtr context = conn->GetContext<RtmpContext>(kRtmpContext);
    if (context)
    {
        int ret = context->Parse(buf); // 开始握手
        if (ret == 0)                // 0握手成功
        {
            RTMP_DEBUG("host:{} handshake success.", conn->PeerAddr().ToIpPort());
        }
        else if (ret == -1) // -1失败
        {
            conn->ForceClose();
        }
    }
}

/// @brief 写完成之后，就要改变握手状态机的状态
/// @param conn
void RtmpServer::OnWriteComplete(const ConnectionPtr& conn)
{
    RtmpContextPtr shake = conn->GetContext<RtmpContext>(kRtmpContext);
    if (shake)
    {
        shake->OnWriteComplete(); // 运转状态机
    }
}

void RtmpServer::OnActive(const ConnectionPtr& conn)
{
    if (rtmp_handler_)
    {
        rtmp_handler_->OnActive(conn);
    }
}

} // namespace tmms::mm