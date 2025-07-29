/*
 * @Author: star-cs
 * @Date: 2025-07-28 15:22:03
 * @LastEditTime: 2025-07-28 15:22:07
 * @FilePath: /TMMS-SERVER/tmms/mmedia/rtmp/rtmp_context.cpp
 * @Description:
 */
#include "rtmp_context.h"
#include "base/log/log.h"
#include "mmedia/base/bytes_reader.h"
#include "mmedia/base/packet.h"
#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "mmedia/rtmp/rtmp_hander.h"
#include "network/net/tcpconnection.h"
#include <cstdint>
#include <cstring>
#include <memory>

namespace tmms::mm
{
RtmpContext::RtmpContext(const TcpConnectionPtr& con, RtmpHandler* handler, bool client)
    : handshake_(con, client),
      connection_(con),
      rtmp_handler_(handler)
{
}

/// @brief 状态机解析rtmp头
/// @param buf
/// @return
int32_t RtmpContext::Parse(MsgBuffer& buf)
{
    int32_t ret = 0;
    if (state_ == kRtmpHandShake)
    {
        ret = handshake_.HandShake(buf);
        if (ret == 0)
        {
            state_ = kRtmpMessage;
            if (buf.ReadableBytes() > 0)
            {
                return Parse(buf);
            }
        }
        else if (ret == -1)
        {
            RTMP_TRACE("RTMP handshake error");
        }
        else if (ret == 2)
        {
            state_ = kRtmpWatingDone;
        }
    }
    else if (state_ == kRtmpMessage)
    {
        return ParseMessage(buf);
    }

    return ret;
}

/// @brief 调用握手的writercomplete回调函数，握手
void RtmpContext::OnWriteComplete()
{
    if (state_ == kRtmpHandShake)
    {
        handshake_.WriteComplete();
    }
    else if (state_ == kRtmpWatingDone)
    {
        state_ = kRtmpMessage;
    }
    else if (state_ == kRtmpMessage) {}
}

/// @brief  启动握手，调用handshake_类的开始，客户端开始发送C0C1，服务端等待
void RtmpContext::StarHandShake()
{
    handshake_.Start();
}

/// @brief 解析rtmp消息包，header + body
/// @param buf
/// @return 1:数据不足；
int32_t RtmpContext::ParseMessage(MsgBuffer& buf)
{
    uint8_t  fmt; // 一个字节
    uint32_t csid, msg_len = 0, msg_sid = 0, timestamp = 0;
    uint8_t  msg_type    = 0;
    uint32_t total_bytes = buf.ReadableBytes(); // 包的大小
    int32_t  parsed      = 0;                   // 已经解析的字节数

    while (total_bytes > 1)
    {
        const char* pos = buf.Peek();
        parsed          = 0;
        //*********basic header********* */
        fmt  = (*pos >> 6) & 0x03;
        csid = *pos & 0x3F;
        parsed++;
        // csid 取0，1是一个新的消息包
        // csid 如果是2，就是使用上一个包的所有东西，不用更新csid
        if (csid == 0)
        {
            if (total_bytes < 2)
            {
                return 1;
            }
            csid = 64 + *((uint8_t*)(pos + parsed));
            parsed++;
        }
        else if (csid == 1)
        {
            if (total_bytes < 3)
            {
                return 1;
            }
            csid = 64 + *((uint8_t*)(pos + parsed));
            parsed++;
            csid = (csid << 8) + *((uint8_t*)(pos + parsed));
            parsed++;
        }

        int size = total_bytes - parsed;
        if (size == 0 || (fmt == 0 && size < 11) || (fmt == 1 && size < 7) || (fmt == 2 && size < 3) ||
            (fmt == 3 && size < 1))
        {
            return 1;
        }

        //*********message header********* */
        msg_len    = 0;
        msg_sid    = 0;
        msg_type   = 0;
        timestamp  = 0;
        int32_t ts = 0; // 时间差

        RtmpMsgHeaderPtr& prev = in_message_headers_[csid]; // 取出前一个消息头
        if (!prev)
        {
            prev = std::make_shared<RtmpMsgHeader>();
        }

        if (fmt == kRtmpFmt0)
        {
            // timestamp 是前3字节
            parsed += 3;
            // fmt=0的时候，存的timestamp是绝对时间，所以时间差为0。
            // 其他两个存的是和上一个chunk的时间差
            in_deltas_[csid] = 0;
            timestamp        = ts;

            msg_len = BytesReader::ReadUint24T(pos + parsed);
            parsed += 3;
            msg_type = BytesReader::ReadUint8T(pos + parsed);
            parsed += 1;
            msg_sid = BytesReader::ReadUint32T(pos + parsed);
            parsed += 4;
        }
        else if (fmt == kRtmpFmt1)
        {
            // timestamp 是前3字节
            ts = BytesReader::ReadUint24T(pos + parsed);
            parsed += 3;
            in_deltas_[csid] = ts;
            timestamp        = ts + prev->timestamp;
            msg_len          = BytesReader::ReadUint24T(pos + parsed);
            parsed += 3;
            msg_type = BytesReader::ReadUint8T(pos + parsed);
            parsed += 1;
            // fmt=1用前一个消息的stream id
            msg_sid = prev->msg_sid;
        }
        else if (fmt == kRtmpFmt2)
        {
            // timestamp 是前3字节
            ts = BytesReader::ReadUint24T(pos + parsed);
            parsed += 3;
            in_deltas_[csid] = ts;
            timestamp        = ts + prev->timestamp;
            // fmt=2，下面都用前一个
            msg_len  = prev->msg_len;
            msg_type = prev->msg_type;
            msg_sid  = prev->msg_sid;
        }
        else if (fmt == kRtmpFmt3)
        {
            timestamp = in_deltas_[csid] + prev->timestamp;
            msg_len   = prev->msg_len;
            msg_type  = prev->msg_type;
            msg_sid   = prev->msg_sid;
        }

        //*********Ext timestamp********* */
        bool ext = (ts == 0xFFFFFF); // timestamp全1表示有ext
        if (fmt == kRtmpFmt3)
        { // fmt=3，都是用的前一个header,直接沿用就行
            ext = in_ext_[csid];
        }
        in_ext_[csid] = ext;
        if (ext)
        {
            if (total_bytes - parsed < 4)
            {
                return 1;
            }
            timestamp = BytesReader::ReadUint32T(pos + parsed);
            parsed += 4;
            if (fmt != kRtmpFmt0) // 不是fmt0，要计算时间差
            {
                timestamp        = ts + prev->timestamp;
                in_deltas_[csid] = ts;
            }
        }

        // 数据包
        PacketPtr& packet = in_packets_[csid];
        if (!packet)
        {
            packet = Packet::NewPacket(msg_len);
        }
        RtmpMsgHeaderPtr header = packet->Ext<RtmpMsgHeader>(); // 创建数据包头
        if (!header)
        {
            header = std::make_shared<RtmpMsgHeader>();
            packet->SetExt(header);
        }
        header->cs_id     = csid;
        header->timestamp = timestamp;
        header->msg_len   = msg_len;
        header->msg_type  = msg_type;
        header->msg_sid   = msg_sid;

        // chunk body数据
        int bytes = std::min(packet->Space(), in_chunk_size_);
        if (total_bytes - parsed < bytes)
        {
            return 1;
        }

        // chunk data 起始位置
        const char* body = packet->Data() + packet->PacketSize();
        memcpy((void*)body, pos + parsed, bytes);
        packet->UpdatePacketSize(bytes);
        parsed += bytes;

        buf.Retrieve(parsed);
        total_bytes -= parsed;

        // 更新前一个消息头
        prev->cs_id     = csid;
        prev->msg_len   = msg_len;
        prev->msg_sid   = msg_sid;
        prev->msg_type  = msg_type;
        prev->timestamp = timestamp;

        // 一个完整的Message消息收完了
        if (packet->Space() == 0)
        {
            packet->SetPacketType(msg_type);
            packet->SetTimeStamp(timestamp);
            MessageComplete(std::move(packet));
            packet.reset();
        }
    }
    return 1;
}

void RtmpContext::MessageComplete(Packet::ptr&& data)
{
    RTMP_TRACE("recv message type:{}, len:{}", data->PacketType(), data->PacketSize());
}
} // namespace tmms::mm