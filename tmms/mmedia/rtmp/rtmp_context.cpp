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
#include "mmedia/base/bytes_writer.h"
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
        auto i     = ParseMessage(buf);
        last_left_ = buf.ReadableBytes();
        return i;
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
    else if (state_ == kRtmpMessage)
    {
        CheckAndSend(); // 写完后，重置 sending_ ，判断是否还需要发送消息
    }
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

    // 上次剩下的数据不使用了，记录当前包的数据大小
    in_bytes_ += (buf.ReadableBytes() - last_left_); // 再次收到的消息字节数
    SendBytesRecv();

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
    auto type = data->PacketType();
    switch (type)
    {
        case kRtmpMsgTypeChunkSize:
        {
            HandleChunkSize(data);
            break;
        }
        case kRtmpMsgTypeBytesRead:
        {
            RTMP_TRACE("message bytes read recv.");
            break;
        }
        case kRtmpMsgTypeUserControl:
        {
            HandleUserMessage(data);
            break;
        }
        case kRtmpMsgTypeWindowACKSize:
        {
            HandleAckWindowSize(data);
            break;
        }
        default:
            RTMP_ERROR("not surpport message type:{}", type);
            break;
    }
}

bool RtmpContext::BuildChunk(const PacketPtr& packet, uint32_t timestamp, bool fmt0)
{
    RtmpMsgHeaderPtr header = packet->Ext<RtmpMsgHeader>();
    if (header)
    {
        out_sending_packets_.emplace_back(packet);
        RtmpMsgHeaderPtr& prev = out_message_headers_[header->cs_id]; // 取出前一个发送的消息头
        // 不是fmt0的条件
        // 1. 不指定 fmt0 = false
        // 2. 有前一个消息头
        // 3. 当前时间戳 > 前一个消息的时间戳
        // 4. 当前的msg_id 和 前一个一样，那么不是
        bool use_delta = !fmt0 && prev && timestamp >= prev->timestamp && header->msg_sid == prev->msg_sid;
        if (!prev)
        {
            prev = std::make_shared<RtmpMsgHeader>();
        }
        int fmt = kRtmpFmt0;
        if (use_delta)
        { // 不是 fmt0
            fmt = kRtmpFmt1;
            timestamp -= prev->timestamp; // 时间差
            // type len 至少是2
            if (header->msg_len == prev->msg_len && header->msg_type == prev->msg_type)
            {
                fmt = kRtmpFmt2;
                if (timestamp == out_deltas_[header->cs_id])
                { // fmt3的deltas和前一个相同
                    fmt = kRtmpFmt3;
                }
            }
        }

        char* p = out_current_;
        if (header->cs_id < 64)
        {
            *p++ = (char)((fmt << 6) | header->cs_id); // 2位fmt，6位csid
        }
        else if (header->cs_id < (64 + 256))
        {
            *p++ = (char)((fmt << 6) | 0);
            *p++ = (char)(header->cs_id - 64);
        }
        else
        {
            *p++        = (char)((fmt << 6) | 1);
            uint16_t cs = header->cs_id - 64;
            memcpy(p, &cs, sizeof(uint16_t));
            p += sizeof(uint16_t);
        }

        auto ts = timestamp;
        if (ts >= 0xFFFFFF)
        { // 有没有使用额外的timestamp
            ts = 0xFFFFFF;
        }

        // 组装header
        if (fmt == kRtmpFmt0)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            p += BytesWriter::WriteUint24T(p, header->msg_len); // msg_len 3
            p += BytesWriter::WriteUint8T(p, header->msg_type); // msg_type 1

            memcpy(p, &header->msg_sid, 4); // streamid传输也是小端存放，不需要转换
            p += 4;
            out_deltas_[header->cs_id] = 0;
        }
        else if (fmt == kRtmpFmt1)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            p += BytesWriter::WriteUint24T(p, header->msg_len); // msg_len 3
            p += BytesWriter::WriteUint8T(p, header->msg_type); // msg_type 1
            out_deltas_[header->cs_id] = timestamp;
        }
        else if (fmt == kRtmpFmt2)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            out_deltas_[header->cs_id] = timestamp;
        }
        // kRtmpFmt3不需要填header

        if (0xFFFFFF == ts) // 有扩展的，就直接再拷贝四字节
        {
            memcpy(p, &timestamp, 4);
            p += 4;
        }
        // 起始out_current_， 包大小p - out_current_
        BufferNodePtr nheader = std::make_shared<BufferNode>(out_current_, p - out_current_);
        sending_bufs_.emplace_back(nheader);
        out_current_ = p;

        prev->cs_id    = header->cs_id; // 更新前一个的值
        prev->msg_len  = header->msg_len;
        prev->msg_sid  = header->msg_sid;
        prev->msg_type = header->msg_type;
        if (fmt == kRtmpFmt0) // fmt=0就是当前的时间
        {
            prev->timestamp = timestamp;
        }
        else
        {
            prev->timestamp += timestamp;
        }

        // packet 存储 body
        const char* body         = packet->Data();
        int32_t     bytes_parsed = 0;

        // 这里开始是一直发，直到数据包发完为止
        while (true)
        {
            //////////  复制body数据
            const char* chunk = body + bytes_parsed;            // chunk的起始地址
            int32_t     size  = header->msg_len - bytes_parsed; // 还剩下多少没解析组装
            size              = std::min(size, out_chunk_size_);

            // header和body分开发送
            BufferNodePtr node = std::make_shared<BufferNode>((void*)chunk, size);
            sending_bufs_.emplace_back(std::move(node));
            bytes_parsed += size;
            ///////// 还有数据，不断解析头部，然后循环上面解析body
            if (bytes_parsed < header->msg_len) // 还没有解析完
            {
                if (out_current_ - out_buffer_ >= 4096) // 超出out_buffer_的容量了
                {
                    RTMP_ERROR("rtmp had no enough out header buffer.");
                    break;
                }
                // 还有数据， 那就说明是下一个数据包了，还要解析csid
                // 这里默认fm0之后发送fm3
                char* p = out_current_;
                if (header->cs_id < 64) // fmt==2的时候，scid只有6位
                {
                    // 0xC0 == 11000000
                    *p++ = (char)((0xC0) | header->cs_id); // 前2位fmt，后6位csid
                }
                else if (header->cs_id < (64 + 256))
                {
                    *p++ = (char)((0xC0) | 0); // 第一个字节前2位是fmt，后6位全为0
                    *p++ = (char)(header->cs_id - 64);
                }
                else
                {
                    *p++        = (char)((0xC0) | 1);
                    uint16_t cs = header->cs_id - 64;
                    memcpy(p, &cs, sizeof(uint16_t)); // 两个字节，拷贝
                    p += sizeof(uint16_t);
                }
                if (0xFFFFFF == ts) // 有扩展的，就直接再拷贝四字节
                {
                    memcpy(p, &timestamp, 4);
                    p += 4;
                }
                BufferNodePtr nheader = std::make_shared<BufferNode>(out_current_, p - out_current_);
                sending_bufs_.emplace_back(std::move(nheader)); // sending_bufs_统一用Send发送
                out_current_ = p;
            }
            else // 数据处理完了，跳出循环
            {
                break;
            }
        }
        return true;
    }
    return false;
}

void RtmpContext::Send()
{
    if (sending_) // 发送状态中不能再发送，一次一个包
    {
        return;
    }

    sending_ = true;
    for (int i = 0; i < 10; ++i) // 一次最多发送10个包
    {
        if (out_waiting_queue_.empty())
        {
            break;
        }

        PacketPtr packet = std::move(out_waiting_queue_.front());
        out_waiting_queue_.pop_front();
        // 网络接收的包，拿出来进行组装
        BuildChunk(std::move(packet)); // 拿数据包构建chunk到sending_bufs_
    }

    connection_->Send(sending_bufs_);
}

bool RtmpContext::Ready() const
{
    return !sending_;
}

bool RtmpContext::BuildChunk(PacketPtr&& packet, uint32_t timestamp, bool fmt0)
{
    RtmpMsgHeaderPtr header = packet->Ext<RtmpMsgHeader>();
    if (header)
    {
        out_sending_packets_.emplace_back(std::move(packet));
        RtmpMsgHeaderPtr& prev = out_message_headers_[header->cs_id]; // 取出前一个发送的消息头
        // 不是fmt0的条件
        // 1. 不指定 fmt0 = false
        // 2. 有前一个消息头
        // 3. 当前时间戳 > 前一个消息的时间戳
        // 4. 当前的msg_id 和 前一个一样，那么不是
        bool use_delta = !fmt0 && prev && timestamp >= prev->timestamp && header->msg_sid == prev->msg_sid;
        if (!prev)
        {
            prev = std::make_shared<RtmpMsgHeader>();
        }
        int fmt = kRtmpFmt0;
        if (use_delta)
        { // 不是 fmt0
            fmt = kRtmpFmt1;
            timestamp -= prev->timestamp; // 时间差
            // type len 至少是2
            if (header->msg_len == prev->msg_len && header->msg_type == prev->msg_type)
            {
                fmt = kRtmpFmt2;
                if (timestamp == out_deltas_[header->cs_id])
                { // fmt3的deltas和前一个相同
                    fmt = kRtmpFmt3;
                }
            }
        }

        char* p = out_current_;
        if (header->cs_id < 64)
        {
            *p++ = (char)((fmt << 6) | header->cs_id); // 2位fmt，6位csid
        }
        else if (header->cs_id < (64 + 256))
        {
            *p++ = (char)((fmt << 6) | 0);
            *p++ = (char)(header->cs_id - 64);
        }
        else
        {
            *p++        = (char)((fmt << 6) | 1);
            uint16_t cs = header->cs_id - 64;
            memcpy(p, &cs, sizeof(uint16_t));
            p += sizeof(uint16_t);
        }

        auto ts = timestamp;
        if (ts >= 0xFFFFFF)
        { // 有没有使用额外的timestamp
            ts = 0xFFFFFF;
        }

        // 组装header
        if (fmt == kRtmpFmt0)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            p += BytesWriter::WriteUint24T(p, header->msg_len); // msg_len 3
            p += BytesWriter::WriteUint8T(p, header->msg_type); // msg_type 1

            memcpy(p, &header->msg_sid, 4); // streamid传输也是小端存放，不需要转换
            p += 4;
            out_deltas_[header->cs_id] = 0;
        }
        else if (fmt == kRtmpFmt1)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            p += BytesWriter::WriteUint24T(p, header->msg_len); // msg_len 3
            p += BytesWriter::WriteUint8T(p, header->msg_type); // msg_type 1
            out_deltas_[header->cs_id] = timestamp;
        }
        else if (fmt == kRtmpFmt2)
        {
            p += BytesWriter::WriteUint24T(p, ts);
            out_deltas_[header->cs_id] = timestamp;
        }
        // kRtmpFmt3不需要填header

        if (0xFFFFFF == ts) // 有扩展的，就直接再拷贝四字节
        {
            memcpy(p, &timestamp, 4);
            p += 4;
        }
        // 起始out_current_， 包大小p - out_current_
        BufferNodePtr nheader = std::make_shared<BufferNode>(out_current_, p - out_current_);
        sending_bufs_.emplace_back(nheader);
        out_current_ = p;

        prev->cs_id    = header->cs_id; // 更新前一个的值
        prev->msg_len  = header->msg_len;
        prev->msg_sid  = header->msg_sid;
        prev->msg_type = header->msg_type;
        if (fmt == kRtmpFmt0) // fmt=0就是当前的时间
        {
            prev->timestamp = timestamp;
        }
        else
        {
            prev->timestamp += timestamp;
        }

        // packet 存储 body
        const char* body         = packet->Data();
        int32_t     bytes_parsed = 0;

        // 这里开始是一直发，直到数据包发完为止
        while (true)
        {
            //////////  复制body数据
            const char* chunk = body + bytes_parsed;            // chunk的起始地址
            int32_t     size  = header->msg_len - bytes_parsed; // 还剩下多少没解析组装
            size              = std::min(size, out_chunk_size_);

            // header和body分开发送
            BufferNodePtr node = std::make_shared<BufferNode>((void*)chunk, size);
            sending_bufs_.emplace_back(std::move(node));
            bytes_parsed += size;
            ///////// 还有数据，不断解析头部，然后循环上面解析body
            if (bytes_parsed < header->msg_len) // 还没有解析完
            {
                if (out_current_ - out_buffer_ >= 4096) // 超出out_buffer_的容量了
                {
                    RTMP_ERROR("rtmp had no enough out header buffer.");
                    break;
                }
                // 还有数据， 那就说明是下一个数据包了，还要解析csid
                // 这里默认fm0之后发送fm3
                char* p = out_current_;
                if (header->cs_id < 64) // fmt==2的时候，scid只有6位
                {
                    // 0xC0 == 11000000
                    *p++ = (char)((0xC0) | header->cs_id); // 前2位fmt，后6位csid
                }
                else if (header->cs_id < (64 + 256))
                {
                    *p++ = (char)((0xC0) | 0); // 第一个字节前2位是fmt，后6位全为0
                    *p++ = (char)(header->cs_id - 64);
                }
                else
                {
                    *p++        = (char)((0xC0) | 1);
                    uint16_t cs = header->cs_id - 64;
                    memcpy(p, &cs, sizeof(uint16_t)); // 两个字节，拷贝
                    p += sizeof(uint16_t);
                }
                if (0xFFFFFF == ts) // 有扩展的，就直接再拷贝四字节
                {
                    memcpy(p, &timestamp, 4);
                    p += 4;
                }
                BufferNodePtr nheader = std::make_shared<BufferNode>(out_current_, p - out_current_);
                sending_bufs_.emplace_back(std::move(nheader)); // sending_bufs_统一用Send发送
                out_current_ = p;
            }
            else // 数据处理完了，跳出循环
            {
                break;
            }
        }
        return true;
    }
    return false;
}

void RtmpContext::CheckAndSend()
{
    sending_     = false;
    out_current_ = out_buffer_;
    sending_bufs_.clear();
    out_sending_packets_.clear();

    if (!out_waiting_queue_.empty()) // 还有数据
    {
        Send();
    }
    else
    {
        if (rtmp_handler_) // 发送完了，让上层处理
        {
            rtmp_handler_->OnActive(connection_);
        }
    }
}

void RtmpContext::PushOutQueue(PacketPtr&& packet)
{
    out_waiting_queue_.push_back(std::move(packet));
    Send();
}

/// 控制 msg_id 都是0，cs_id 都是 2
void RtmpContext::SendSetChunkSize() // 块大小
{
    PacketPtr        packet = Packet::NewPacket(64);             // 创建消息包
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>(); // 创建消息头
    if (header)
    {
        header->cs_id     = kRtmpCSIDCommand; // 所有控制命令的scid都是2
        header->msg_type  = kRtmpMsgTypeChunkSize;
        header->timestamp = 0;
        header->msg_sid   = kRtmpMsID0;
        header->timestamp = 0;

        packet->SetExt(header);

        char* body = packet->Data();

        header->msg_len = BytesWriter::WriteUint32T(body, out_chunk_size_);
        packet->SetPacketSize(header->msg_len); // 最大包的大小

        RTMP_DEBUG("send chunk size:{}, to host:{}", out_chunk_size_, connection_->PeerAddr().ToIpPort());
        PushOutQueue(std::move(packet));
    }
}

void RtmpContext::SendAckWindowsSize() // 确认窗口大小
{
    PacketPtr        packet = Packet::NewPacket(64);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    if (header)
    {
        header->cs_id     = kRtmpCSIDCommand;
        header->msg_len   = 4;
        header->msg_type  = kRtmpMsgTypeWindowACKSize;
        header->msg_sid   = kRtmpMsID0;
        header->timestamp = 0;

        packet->SetExt(header);

        char* body = packet->Data();
        BytesWriter::WriteUint32T(body, ack_size_);
        packet->SetPacketSize(header->msg_len);

        RTMP_DEBUG("send ack size:{}, to host:{}", ack_size_, connection_->PeerAddr().ToIpPort());
        PushOutQueue(std::move(packet));
    }
}

void RtmpContext::SendSetPeerBandwidth() // 带宽，和窗口大小值是一样的
{
    PacketPtr        packet = Packet::NewPacket(64);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    if (header)
    {
        header->cs_id     = kRtmpCSIDCommand;
        header->msg_len   = 5;
        header->msg_type  = kRtmpMsgTypeSetPeerBW;
        header->msg_sid   = kRtmpMsID0;
        header->timestamp = 0;

        packet->SetExt(header);

        char* body = packet->Data();
        body += BytesWriter::WriteUint32T(body, ack_size_);
        *body++ = 0x02; // 0x02: limit type
        packet->SetPacketSize(header->msg_len);

        RTMP_DEBUG("send PeerBandwidth:{}, to host:{}", ack_size_, connection_->PeerAddr().ToIpPort());
        PushOutQueue(std::move(packet));
    }
}

/// @brief 发送确认消息, 客户端或者服务器在收到窗口大小的字节后必须发送给对端一个确认消息
/// @param bytes
void RtmpContext::SendBytesRecv() // 发送确认消息
{
    if (in_bytes_ >= ack_size_) // 收到的数据已经大于等于确认窗口了，需要发一个确认消息
    {
        PacketPtr        packet = Packet::NewPacket(64);
        RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
        if (header)
        {
            header->cs_id     = kRtmpCSIDCommand;
            header->msg_len   = 4;
            header->msg_type  = kRtmpMsgTypeBytesRead;
            header->msg_sid   = kRtmpMsID0;
            header->timestamp = 0;

            packet->SetExt(header);

            char* body = packet->Data();
            body += BytesWriter::WriteUint32T(body, in_bytes_);
            packet->SetPacketSize(header->msg_len);

            RTMP_DEBUG("send BytesRecv:{}, to host:{}", in_bytes_, connection_->PeerAddr().ToIpPort());
            PushOutQueue(std::move(packet));
            in_bytes_ = 0;
        }
    }
}

/// @brief
/// 发送控制消息用户控制消息用于告知对方执行该信息中包含的用户控制事件，消息类型为4，并且MesageStreamID使用0，Chumnk
/// StrcamID固定使用2。
/// @param nType  控制事件类型
/// @param value1
/// @param value2
void RtmpContext::SendUserCtrlMessage(short nType, uint32_t value1, uint32_t value2) // 发送用户控制消息
{
    PacketPtr        packet = Packet::NewPacket(64);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    if (header)
    {
        header->cs_id     = kRtmpCSIDCommand;
        header->msg_len   = 0;
        header->msg_type  = kRtmpMsgTypeUserControl;
        header->timestamp = 0;
        header->msg_sid   = kRtmpMsID0;
        packet->SetExt(header);

        char* body = packet->Data();
        char* p    = body;
        p += BytesWriter::WriteUint16T(p, value1);
        p += BytesWriter::WriteUint16T(p, value2);
        if (nType == kRtmpEventTypeSetBufferLength)
        {
            p += BytesWriter::WriteUint16T(p, value2);
        }
        header->msg_len = p - body;
        packet->SetPacketSize(header->msg_len);

        RTMP_DEBUG(
            "send UserCtrlMessage nType:{} value1:{} value2:{} to host:{}",
            in_bytes_,
            value1,
            value2,
            connection_->PeerAddr().ToIpPort());
        PushOutQueue(std::move(packet));
    }
}

// 接收处理
void RtmpContext::HandleChunkSize(PacketPtr& packet)
{
    if (packet->PacketSize() >= 4) // chunkSize至少4字节
    {
        auto size = BytesReader::ReadUint32T(packet->Data());
        RTMP_DEBUG("recv chunk size in_chunk_size:{} change to:{}", in_chunk_size_, size);
        in_chunk_size_ = size;
    }
    else
    {
        RTMP_ERROR(
            "invalid chunk size packet msg_len:{}  host:{}", packet->PacketSize(), connection_->PeerAddr().ToIpPort());
    }
}
void RtmpContext::HandleAckWindowSize(PacketPtr& packet)
{
    if (packet->PacketSize() >= 4) // chunkSize至少4字节
    {
        auto size = BytesReader::ReadUint32T(packet->Data());
        RTMP_DEBUG("recv ack window size in_chunk_size:{} change to:{}", in_chunk_size_, size);
        ack_size_ = size;
    }
    else
    {
        RTMP_ERROR(
            "invalid chunk size packet msg_len:{}  host:{}", packet->PacketSize(), connection_->PeerAddr().ToIpPort());
    }
}

/// @brief 用户控制消息解析
/// @param packet
void RtmpContext::HandleUserMessage(PacketPtr& packet)
{
    auto msg_len = packet->PacketSize();
    if (msg_len < 6)
    {
        RTMP_ERROR(
            "invalid user control packet msg_len:{}, host:{}",
            packet->PacketSize(),
            connection_->PeerAddr().ToIpPort());
        return;
    }

    char* body = packet->Data();
    // 1.解析类型
    auto type = BytesReader::ReadUint16T(body);
    body += 2;
    // 2.解析用户数据 4字节
    auto value = BytesReader::ReadUint32T(body);

    RTMP_TRACE("recv user control type:{} host:{}", value, connection_->PeerAddr().ToIpPort());

    switch (type)
    {
        case kRtmpEventTypeStreamBegin: // 流开始
        {
            RTMP_TRACE("recv stream begin value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            break;
        }
        case kRtmpEventTypeStreamEOF: // 流结束
        {
            RTMP_TRACE("recv stream eof value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            break;
        }
        case kRtmpEventTypeStreamDry: // 流变为空（无数据）
        {
            RTMP_TRACE("recv stream dry value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            break;
        }
        case kRtmpEventTypeSetBufferLength: // 设置缓冲长度
        {
            RTMP_TRACE("recv set buffer length value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            if (msg_len < 10)
            {
                RTMP_ERROR(
                    "invalid user control packet msg_len::{} host:{}",
                    packet->PacketSize(),
                    connection_->PeerAddr().ToIpPort());
                return;
            }
            break;
        }
        case kRtmpEventTypeStreamsRecorded: // 录制的流
        {
            RTMP_TRACE("recv stream recoded value:{} host:{}", value, connection_->PeerAddr().ToIpPort());

            break;
        }
        case kRtmpEventTypePingRequest: // Ping 请求
        {
            RTMP_TRACE("recv ping request value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            SendUserCtrlMessage(kRtmpEventTypePingResponse, value, 0);
            break;
        }
        case kRtmpEventTypePingResponse: // Ping 响应
        {
            RTMP_TRACE("recv ping response value:{} host:{}", value, connection_->PeerAddr().ToIpPort());
            break;
        }
        default:
            break;
    }
}

} // namespace tmms::mm