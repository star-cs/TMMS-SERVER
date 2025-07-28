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
    RtmpHandShake::ptr shake = std::make_shared<RtmpHandShake>(conn, false);
    conn->SetContext(kRtmpContext, shake);
    shake->Start();
    // RtmpContext::ptr shake = std::make_shared<RtmpContext>(conn, nullptr);
    // conn->SetContext(kRtmpContext, shake);
    // shake->StarHandShake(); // 等待C0C1
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
    auto shake = conn->GetContext<RtmpHandShake>(kRtmpContext);
    if (shake)
    {
        auto ret = shake->HandShake(buf);
        if (ret == 0)
        {
            RTMP_TRACE("host: {} handshake success", conn->PeerAddr().ToIpPort());
        }
        else if (ret == -1)
        {
            conn->ForceClose();
        }
    }
}

void RtmpServer::OnWriteComplete(const ConnectionPtr& conn)
{
    auto shake = conn->GetContext<RtmpHandShake>(kRtmpContext);
    if (shake)
    {
        shake->WriteComplete();
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