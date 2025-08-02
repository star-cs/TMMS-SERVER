#pragma once
#include "mmedia/base/packet.h"
#include "mmedia/rtmp/rtmp_handler.h"
#include "network/net/tcpclient.h"

using namespace tmms::net;

namespace tmms::mm
{
// 使用 包含 而不是继承 TcpClient，可以见CreateTcpClient，方便每次修改源时，直接修改成员变量
// 如果是继承，那么就得一开始提供地址（地址需要从推流地址中获取）
class RtmpClient
{
public:
    RtmpClient(EventLoop* loop, RtmpHandler* handler);
    ~RtmpClient();

public:
    void SetCloseCallback(const CloseConnectionCallback& cb);
    void SetCloseCallback(CloseConnectionCallback&& cb);

    void Play(const std::string& url);    // 播放（拉流）
    void Publish(const std::string& url); // 推流
    void Send(PacketPtr&& data);          // 推流发送

private:
    void OnWriteComplete(const TcpConnectionPtr& conn);              // 写完成回调
    void OnConnection(const TcpConnectionPtr& conn, bool connected); // 连接回调
    void OnMessage(const TcpConnectionPtr& conn, MsgBuffer& uf);

    bool ParseUrl(const std::string& url);
    void CreateTcpClient();

    EventLoop*              loop_{nullptr};
    InetAddress             addr_; // 服务端地址
    RtmpHandler*            handler_{nullptr};
    TcpClientPtr            tcp_client_; // 使用tcpclient进行数据的连接接受发送
    std::string             url_;
    bool                    is_player_{false};
    CloseConnectionCallback close_cb_;
};

} // namespace tmms::mm