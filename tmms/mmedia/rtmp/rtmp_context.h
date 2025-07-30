/*
 * @Author: star-cs
 * @Date: 2025-07-28 14:26:02
 * @LastEditTime: 2025-07-28 15:21:39
 * @FilePath: /TMMS-SERVER/tmms/mmedia/rtmp/rtmp_context.h
 * @Description:
 */
#pragma once
#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "mmedia/rtmp/rtmp_hander.h"
#include "mmedia/rtmp/rtmp_handler.h"
#include "network/net/tcpconnection.h"
#include <memory>
#include <queue>
namespace tmms::mm
{
using namespace tmms::net;

enum RtmpContextState
{
    kRtmpHandShake  = 0, // 收到S0S1S2之后转换成下一个状态
    kRtmpWatingDone = 1, // 发送C2状态下一个
    kRtmpMessage    = 2, // 握手接受，数据传输
};

// 枚举定义不同的 用户RTMP 事件类型
enum RtmpEventType
{
    kRtmpEventTypeStreamBegin = 0, // 流开始
    kRtmpEventTypeStreamEOF,       // 流结束
    kRtmpEventTypeStreamDry,       // 流变为空（无数据）
    kRtmpEventTypeSetBufferLength, // 设置缓冲长度
    kRtmpEventTypeStreamsRecorded, // 录制的流
    kRtmpEventTypePingRequest,     // Ping 请求
    kRtmpEventTypePingResponse     // Ping 响应
};

class RtmpContext // rtmp上下文
{
public:
    RtmpContext(const TcpConnectionPtr& con, RtmpHandler* handler, bool client = false);
    ~RtmpContext() = default;

public:
    int32_t Parse(MsgBuffer& buf);
    void    OnWriteComplete();
    void    StarHandShake();

    // 接受数据解析
    int32_t ParseMessage(MsgBuffer& buf);
    void    MessageComplete(Packet::ptr&& data);

    // 发送数据包
    bool BuildChunk(const PacketPtr& packet, uint32_t timestamp = 0, bool fmt0 = false);
    void Send();
    bool Ready() const;

    // 控制消息
    // 发送 rtmp控制消息
    void SendSetChunkSize();                                                 // 块大小
    void SendAckWindowsSize();                                               // 确认窗口大小
    void SendSetPeerBandwidth();                                             // 带宽，和窗口大小值是一样的
    void SendBytesRecv();                                                    // 发送消息
                                                                             // 用户控制消息
    void SendUserCtrlMessage(short nType, uint32_t value1, uint32_t value2); // 发送用户控制消息
                                                                             // 接收处理
    void HandleChunkSize(PacketPtr& packet);
    void HandleAckWindowSize(PacketPtr& packet);
    void HandleUserMessage(PacketPtr& packet);

private:
    bool BuildChunk(PacketPtr&& packet, uint32_t timestamp = 0, bool fmt0 = false);
    void CheckAndSend();
    void PushOutQueue(PacketPtr&& packet);

private:
    RtmpHandShake                                  handshake_; // rtmp握手
    int32_t                                        state_{kRtmpHandShake};
    TcpConnectionPtr                               connection_;
    RtmpHandler*                                   rtmp_handler_{nullptr};
    std::unordered_map<uint32_t, RtmpMsgHeaderPtr> in_message_headers_; // csid作为key, 包头作为value
    std::unordered_map<uint32_t, PacketPtr>        in_packets_; // csid，记录着当前 Message 的所有数据包（chunk data）
    std::unordered_map<uint32_t, uint32_t>         in_deltas_;  // csid，前一个消息的 时间差 （fmt3用的）
    std::unordered_map<uint32_t, bool>             in_ext_;     // 前一个消息的是否有 Extended Timestamp

    int32_t in_chunk_size_{128};

    /////////   发送数据
    char                                           out_buffer_[4096];
    char*                                          out_current_{nullptr}; // 缓存out_buffer当前读的指针
    std::unordered_map<uint32_t, uint32_t>         out_deltas_;           // 发送的deltas
    std::unordered_map<uint32_t, RtmpMsgHeaderPtr> out_message_headers_;
    int32_t                                        out_chunk_size_{4096}; // 最大包的大小
    std::list<PacketPtr>                           out_waiting_queue_;    // 等待build的处理，是存放网络接收的packet
    std::list<BufferNodePtr> sending_bufs_;        // buildChunk的数据写在这里保存等待发送,tcp发送的就是这个
    std::list<PacketPtr>     out_sending_packets_; // 发送的包
    bool                     sending_{false};      // 是否在发送

    /////////  控制消息
    int32_t ack_size_{2500000}; // 确认窗口
    int32_t in_bytes_{0};       // 接收到的数据，每次发送确认消息会重置0
    int32_t last_left_{0};      // 剩下的数据
};

using RtmpContextPtr = std::shared_ptr<RtmpContext>;
} // namespace tmms::mm