/*
 * @Author: star-cs
 * @Date: 2025-07-28 15:22:03
 * @LastEditTime: 2025-07-28 15:22:07
 * @FilePath: /TMMS-SERVER/tmms/mmedia/rtmp/rtmp_context.cpp
 * @Description:
 */
#include "rtmp_context.h"
#include "base/log/log.h"
#include "base/utils/stringutils.h"
#include "mmedia/base/bytes_reader.h"
#include "mmedia/base/bytes_writer.h"
#include "mmedia/base/packet.h"
#include "mmedia/rtmp/amf/amf_any.h"
#include "mmedia/rtmp/amf/amf_object.h"
#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "mmedia/rtmp/rtmp_hander.h"
#include "network/net/tcpconnection.h"
#include <cmath>
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
    // 设置 命令处理函数
    commands_["connect"]      = std::bind(&RtmpContext::HandleConnect, this, std::placeholders::_1);
    commands_["createStream"] = std::bind(&RtmpContext::HandleCreateStream, this, std::placeholders::_1);
    commands_["_result"]      = std::bind(&RtmpContext::HandleResult, this, std::placeholders::_1);
    commands_["_error"]       = std::bind(&RtmpContext::HandleError, this, std::placeholders::_1);
    commands_["play"]         = std::bind(&RtmpContext::HandlePlay, this, std::placeholders::_1);
    commands_["publish"]      = std::bind(&RtmpContext::HandlePublish, this, std::placeholders::_1);
    out_current_              = out_buffer_;
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
            ts = BytesReader::ReadUint24T(pos + parsed);
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
        case kRtmpMsgTypeAMFMessage:
        {
            HandleAmfCommand(data, false);
            break;
        }
        case kRtmpMsgTypeAMF3Message:
        {
            HandleAmfCommand(data, true);
            break;
        }
        case kRtmpMsgTypeAMFMeta: // 都是一样的转换视频
        case kRtmpMsgTypeAMF3Meta:
        case kRtmpMsgTypeAudio: // 音频
        case kRtmpMsgTypeVideo: // 视频
        {
            SetPacketType(data); // 转换类型
            // 给业务层包，数据已经解析过了
            if (rtmp_handler_)
            {
                rtmp_handler_->OnRecv(connection_, std::move(data));
            }
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
        out_sending_packets_.emplace_back(std::move(packet));
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

// AMF相关
void RtmpContext::HandleAmfCommand(PacketPtr& data, bool amf3)
{
    RTMP_TRACE("amf message len:{}, host:{}", data->PacketSize(), connection_->PeerAddr().ToIpPort());

    const char* body    = data->Data();
    int32_t     msg_len = data->PacketSize();

    // amf3在开头多了一个字节，表示是amf3，剩下的就是amf0
    if (amf3)
    {
        body += 1;
        msg_len -= 1;
    }
    AMFObject obj;
    if (obj.Decode(body, msg_len) < 0)
    {
        RTMP_TRACE("amf decode error. host:{}", connection_->PeerAddr().ToIpPort());
        return;
    }
    // obj.Dump();

    // 获取消息命令名称，在封装的时候把name写在了amf的第一个
    const std::string& cmd = obj.Property(0)->String();
    RTMP_TRACE("amf command:{} host:{}", cmd, connection_->PeerAddr().ToIpPort());

    auto iter = commands_.find(cmd);
    if (iter == commands_.end())
    {
        RTMP_TRACE("command not found:{} host:{}", cmd, connection_->PeerAddr().ToIpPort());
        return;
    }
    iter->second(obj);
}

// NetConnection相关 服务端和客户端之间进行网络连接的高级表现形式
void RtmpContext::SendConnect()
{
    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    header->msg_sid         = 0;
    header->msg_type        = kRtmpMsgTypeAMFMessage; // AMF0，消息使用AMF序列化
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    // 消息体
    p += AMFObject::EncodeString(p, "connect");
    p += AMFObject::EncodeNumber(p, 1.0);
    *p++ = kAMFObject;
    // command object
    p += AMFObject::EncodeNamedString(p, "app", app_);     // 推流点
    p += AMFObject::EncodeNamedString(p, "tcUrl", tcUrl_); // 推流地址
    p += AMFAny::EncodeNamedBoolean(p, "fpad", false);     //
    p += AMFAny::EncodeNamedNumber(p, "capabilities", 31.0);
    p += AMFAny::EncodeNamedNumber(p, "audioCodecs", 1639.0); // 可以支持的音频类型，二进制1表示
    p += AMFAny::EncodeNamedNumber(p, "videoCodecs", 252.0);  // 同样可支持的视频编码类型
    p += AMFAny::EncodeNamedNumber(p, "videoFunction", 1.0);

    // 对象类型结束 0x00 0x00 0x09
    *p++ = 0x00;
    *p++ = 0x00; // AMF
    *p++ = 0x09;

    // 设置包长度
    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);
    RTMP_TRACE("send connect msg_len: {}  host:{}", header->msg_len, connection_->PeerAddr().ToIpPort());

    // 加入到待编码的队列中
    PushOutQueue(std::move(packet));
}

void RtmpContext::HandleConnect(AMFObject& obj)
{
    auto amf3 = false;
    // 获取command object，在第二个位置（0开始）
    AMFObjectPtr sub_obj = obj.Property(2)->Object();
    if (sub_obj)
    {
        app_   = sub_obj->Property("app")->String();
        tcUrl_ = sub_obj->Property("tcUrl")->String();
        if (sub_obj->Property("objectEncoding"))
        {
            amf3 = sub_obj->Property("objectEncoding")->Number() == 3.0;
        }
    }

    RTMP_TRACE("recv connect tcUrl:{} app:{} amf3:{}", tcUrl_, app_, amf3);

    // 服务端收到 new connect，需要 按顺序 发送三类控制消息初始化连接参数
    SendAckWindowsSize();   // 窗口确认大小
    SendSetPeerBandwidth(); // 设置对端带宽
    SendSetChunkSize();     // 设置chunkSize

    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();

    header->cs_id    = kRtmpCSIDAMFIni; //
    header->msg_sid  = 0;               // 固定的值在connect中
    header->msg_len  = 0;
    header->msg_type = kRtmpMsgTypeAMFMessage; // AMF0，消息使用AMF序列化

    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    p += AMFAny::EncodeString(p, "_result"); // 出错回复_error, 正常回复_result
    p += AMFAny::EncodeNumber(p, 1.0);

    *p++ = kAMFObject;
    p += AMFAny::EncodeNamedString(p, "fmsVer", "FMS/3,0,1,123"); // 版本号
    p += AMFAny::EncodeNamedNumber(p, "capabilities", 31);
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x09;

    *p++ = kAMFObject;
    p += AMFAny::EncodeNamedString(p, "level", "status");
    p += AMFAny::EncodeNamedString(p, "code", "NetConnection.Connect.Success"); // 连接成功
    p += AMFAny::EncodeNamedString(p, "description", "Connection succeeded.");
    p += AMFAny::EncodeNamedNumber(p, "objectEncoding", amf3 ? 3.0 : 0);
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x09;

    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);

    RTMP_TRACE("response connect result msg_len:{} host:{}", header->msg_len, connection_->PeerAddr().ToIpPort());

    // 加入到待编码的队列中
    PushOutQueue(std::move(packet));
}

/// @brief 发送给客户端通知，创建了一个NetStream,客户端收到发起play命令
void RtmpContext::SendCreateStream()
{
    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::shared_ptr<RtmpMsgHeader>();

    header->cs_id    = kRtmpCSIDAMFIni;
    header->msg_sid  = 0;
    header->msg_len  = 0;
    header->msg_type = kRtmpMsgTypeAMFMessage;
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    p += AMFAny::EncodeString(p, "createStream");
    p += AMFAny::EncodeNumber(p, 4.0);
    *p++ = kAMFNull;

    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);
    RTMP_TRACE("send create stream msg_len:{} host:{}", header->msg_len, connection_->PeerAddr().ToIpPort());

    PushOutQueue(std::move(packet));
}

void RtmpContext::HandleCreateStream(AMFObject& obj)
{
    auto tran_id = obj.Property(1)->Number();

    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    header->cs_id           = kRtmpCSIDAMFIni;
    header->msg_sid         = 0;
    header->msg_len         = 0;
    header->msg_type        = kRtmpMsgTypeAMFMessage;
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    p += AMFAny::EncodeString(p, "_result");
    p += AMFAny::EncodeNumber(p, tran_id);
    *p++ = kAMFNull;

    p += AMFAny::EncodeNumber(p, kRtmpMsID1);

    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);

    RTMP_TRACE("response create stream msg_len:{} host:{}", header->msg_len, connection_->PeerAddr().ToIpPort());

    PushOutQueue(std::move(packet));
}

/// @brief NetStream的csid固定为3，ms_id固定为1.类型是20。服务端收到play命令，执行play操作，发送onstatus通知
/// @param level
/// @param code
/// @param description
void RtmpContext::SendStatus(const std::string& level, const std::string& code, const std::string& description)
{
    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    header->cs_id           = kRtmpCSIDAMFIni; // 使用AMF的初始化，固定为3
    header->msg_sid         = 1;               // 固定值
    header->msg_len         = 0;
    header->msg_type        = kRtmpMsgTypeAMFMessage; // AMF0，消息使用AMF序列化
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    // 设置消息体
    p += AMFAny::EncodeString(p, "onStatus"); // command Name
    p += AMFAny::EncodeNumber(p, 0);          // Transaction ID 固定的0
    *p++ = kAMFNull;
    *p++ = kAMFObject;
    // command object
    p += AMFAny::EncodeNamedString(p, "level", level);
    p += AMFAny::EncodeNamedString(p, "code", code);
    p += AMFAny::EncodeNamedString(p, "description", description);

    *p++ = 0x00;
    *p++ = 0x00; // AMF
    *p++ = 0x09;

    // 设置包长度
    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);
    RTMP_TRACE(
        "send status level:{} code:{} description:{} host:{}",
        level,
        code,
        description,
        connection_->PeerAddr().ToIpPort());

    // 加入到待编码的队列中
    PushOutQueue(std::move(packet));
}

void RtmpContext::SendPlay() // 客户端拉流使用
{
    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    header->cs_id           = kRtmpCSIDAMFIni; // 使用AMF的初始化，固定为3
    header->msg_sid         = 1;               // netStream的命令固定为1
    header->msg_len         = 0;
    header->msg_type        = kRtmpMsgTypeAMFMessage; // AMF0，消息使用AMF序列化
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    // 设置消息体
    p += AMFAny::EncodeString(p, "play"); // command Name
    p += AMFAny::EncodeNumber(p, 0);      // Transaction ID 固定的
    *p++ = kAMFNull;                      // 空值
    p += AMFAny::EncodeString(p, name_);

    /**
    Start
    用于指定开始时间（以秒为单位）。默认值为
    -2，表示订阅者首先尝试播放“流名称”字段中指定的直播流。如果未找到同名的直播流，则播放同名的录制流。如果没有找到同名的录制流，则订阅者将等待新的同名直播流，并在可用时播放。如果在“开始”字段中传递
    -1，则仅播放“流名称”字段中指定的直播流。如果在“开始”字段中传递 0
    或正数，则从“开始”字段中指定的时间开始播放“流名称”字段中指定的录制流。如果未找到录制流，则播放播放列表中的下一个项目。
    */
    p += AMFAny::EncodeNumber(p, -1000.0);
    // 设置包长度
    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);

    RTMP_TRACE("send play name:{} msg_len:{} host:{}", name_, header->msg_len, connection_->PeerAddr().ToIpPort());

    // 加入到待编码的队列中
    PushOutQueue(std::move(packet));
}

/// @brief 服务器处理
/// @param obj
void RtmpContext::HandlePlay(AMFObject& obj)
{
    // 1. 收到客户端的play，进行rtmp的url的解析
    auto tran_id = obj.Property(1)->Number();
    name_        = obj.Property(3)->String();
    ParseNameAndTcUrl(); // tcUrl在handleConnect的时候就解析了

    RTMP_TRACE("recv play session_name:{} param:{} host:{}", session_name_, param_, connection_->PeerAddr().ToIpPort());

    is_player_ = true; // 是播放的客户端
    // 2. 解析成功之后，发送streamBegin标志，通知客户端开始播放流
    SendUserCtrlMessage(kRtmpEventTypeStreamBegin, 1, 0);
    // 发送status
    SendStatus("status", "NetStream.Play.Start", "Start playing");

    if (rtmp_handler_) // 如果业务层存在， 告诉业务层
    {
        rtmp_handler_->OnPlay(connection_, session_name_, param_); // 业务层一般创建用户，归纳到推流session中
    }
}

void RtmpContext::ParseNameAndTcUrl() // 解析名称和推流地址
{
    auto pos = app_.find_first_of("/");
    if (pos != std::string::npos)
    {
        app_ = app_.substr(pos + 1);
    }
    param_.clear();
    pos = name_.find_first_of("?"); // 有问号。后面就是带参数的
    if (pos != std::string::npos)
    {
        param_ = name_.substr(pos + 1);
    }

    std::string              domain;
    std::vector<std::string> list = base::StringUtils::SplitString(tcUrl_, "/");
    if (list.size() == 6) // rtmp://ip/domain:port/app/stream
    {
        domain = list[3];
        app_   = list[4];
        name_  = list[5];
    }
    // 没有ip的情况
    if (domain.empty() && tcUrl_.size() > 7) // rtmp:// 7个字符
    {
        auto pos = tcUrl_.find_first_of(":/", 7); // 第一个冒号或斜杠
        if (pos != std::string::npos)
        {
            domain = tcUrl_.substr(7, pos);
        }
    }

    std::stringstream ss;
    session_name_.clear();
    ss << domain << "/" << app_ << "/" << name_;
    session_name_ = ss.str();

    RTMP_TRACE("session_name:{} param:{} host:{}", session_name_, param_, connection_->PeerAddr().ToIpPort());
}

/// @brief  客户端发送推流，发布一个有名字的流到服务器，其他客户端可以用名字进行拉流play
void RtmpContext::SendPublish()
{
    PacketPtr        packet = Packet::NewPacket(1024);
    RtmpMsgHeaderPtr header = std::make_shared<RtmpMsgHeader>();
    header->cs_id           = kRtmpCSIDAMFIni; // 使用AMF的初始化，固定为3
    header->msg_sid         = 1;               // netStream的命令固定为1
    header->msg_len         = 0;
    header->msg_type        = kRtmpMsgTypeAMFMessage; // AMF0，消息使用AMF序列化
    packet->SetExt(header);

    char* body = packet->Data();
    char* p    = body;

    // 设置消息体
    p += AMFAny::EncodeString(p, "publish"); // command Name
    p += AMFAny::EncodeNumber(p, 5);
    *p++ = kAMFNull; // 空值
    p += AMFAny::EncodeString(p, name_);
    p += AMFAny::EncodeString(p, "live"); // 指定 类型，这里是直播
    // 设置包长度
    header->msg_len = p - body;
    packet->SetPacketSize(header->msg_len);

    RTMP_TRACE("send publish name:{} msg_len:{} host:{}", name_, header->msg_len, connection_->PeerAddr().ToIpPort());

    // 加入到待编码的队列中
    PushOutQueue(std::move(packet));
}

/// @brief 服务端处理publish命令，解析参数，拿到tran id回复
/// @param obj
void RtmpContext::HandlePublish(AMFObject& obj)
{
    auto tran_id = obj.Property(1)->Number();
    name_        = obj.Property(3)->String();

    RTMP_TRACE(
        "recv publish session_name:{}  param:{} host:{}", session_name_, param_, connection_->PeerAddr().ToIpPort());

    is_player_ = false; // 已经开始推流就不是拉流的了
    // 通知客户端开始推流
    SendStatus("status", "NetStream.Publish.Start", "Start publishing");

    if (rtmp_handler_)
    {
        // 通知业务层，以后所有的播放都是加入到session_name中
        rtmp_handler_->OnPublish(connection_, session_name_, param_);
    }
}

void RtmpContext::HandleResult(AMFObject& obj)
{
    auto id = obj.Property(1)->Number(); // tran_id
    RTMP_TRACE("recv result id:{}, host:{}", id, connection_->PeerAddr().ToIpPort());
    if (id == 1) // connect
    {
        // id = 1是connect的结果，之后就要进行创建NetStream
        SendCreateStream();
    }
    else if (id == 4) // NetStream结束
    {
        if (is_player_) // 是播放的客户端，就发送play请求
        {
            SendPlay();
        }
        else
        {
            SendPublish(); // 不是播放的，就是发布的
        }
    }
}

void RtmpContext::HandleError(AMFObject& obj)
{
    // 拿到描述
    const std::string& description = obj.Property(3)->Object()->Property("description")->String();
    RTMP_TRACE("recv error description: {} host:{}", description, connection_->PeerAddr().ToIpPort());
    connection_->ForceClose();
}

/// @brief 在收到packet的时候，里面保存的类型是rtmpmsg的类型，需要转换成packetType
/// @param packet
void RtmpContext::SetPacketType(PacketPtr& packet)
{
    if (packet->PacketType() == kRtmpMsgTypeAudio)
    {
        packet->SetPacketType(kPacketTypeAudio);
    }
    if (packet->PacketType() == kRtmpMsgTypeVideo)
    {
        packet->SetPacketType(kPacketTypeVideo);
    }
    if (packet->PacketType() == kRtmpMsgTypeAMFMeta)
    {
        packet->SetPacketType(kPacketTypeMeta);
    }
    if (packet->PacketType() == kRtmpMsgTypeAMF3Meta) // metadata3
    {
        packet->SetPacketType(kPacketTypeMeta3);
    }
}

} // namespace tmms::mm