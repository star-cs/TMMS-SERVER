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
namespace tmms::mm
{
using namespace tmms::net;

enum RtmpContextState
{
    kRtmpHandShake  = 0, // 收到S0S1S2之后转换成下一个状态
    kRtmpWatingDone = 1, // 发送C2状态下一个
    kRtmpMessage    = 2, // 握手接受，数据传输
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

    int32_t ParseMessage(MsgBuffer& buf);
    void    MessageComplete(Packet::ptr&& data);

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
};

using RtmpContextPtr = std::shared_ptr<RtmpContext>;
} // namespace tmms::mm