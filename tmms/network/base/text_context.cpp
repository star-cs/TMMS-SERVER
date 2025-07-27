#include "network/base/text_context.h"
#include <iostream>

namespace tmms::net
{
TextContext::TextContext(const TcpConnectionPtr& con) : connection_(con)
{
}

/// @brief 状态机解析消息
/// @param buf
/// @return 返回1表示需要更多的数据，当前的数据不够；0成功；-1出错
int TextContext::ParseMessage(MsgBuffer& buf)
{
    while (buf.ReadableBytes() > 0)
    {
        if (state_ == kTextContextHeader)
        {
            if (buf.ReadableBytes() >= 4)
            {
                message_length_ = buf.ReadInt32();
                std::cout << "message_length_ : " << message_length_ << std::endl; // todo: 匹配头部字节
                state_ = kTextContextBody;
            }
            else
            {
                return 1;
            }
        }
        else if (state_ == kTextContextBody)
        {
            // 1. 可读数据量大于长度，那就是后面部分是下一个包的长度，只需要读这部分包的长度就行
            if (buf.ReadableBytes() >= message_length_)
            {
                std::string tmp;
                tmp.assign(buf.Peek(), message_length_);
                message_body_.append(tmp);
                buf.Retrieve(message_length_);
                message_length_ = 0;
                if (cb_)
                {
                    cb_(connection_, message_body_);
                    message_body_.clear();
                }

                state_ = kTextContextHeader;
            }
        }
    }
    return 0;
}

} // namespace tmms::net