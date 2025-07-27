/*
 * @Author: star-cs
 * @Date: 2025-07-27 09:57:11
 * @LastEditTime: 2025-07-27 10:11:25
 * @FilePath: /TMMS-SERVER/tmms/network/base/text_context.h
 * @Description: 协议解析
 */
#pragma once

#include "network/net/tcpconnection.h"
namespace tmms::net
{
// 回调到哪个tcpconnect, 解析的消息
using TestMessageCallback = std::function<void(const TcpConnectionPtr&, const std::string&)>;

class TextContext
{
    enum
    {
        kTextContextHeader = 1,
        kTextContextBody   = 2,
    };

public:
    using ptr = std::shared_ptr<TextContext>;

    TextContext(const TcpConnectionPtr& con);
    ~TextContext() = default;

public:
    int  ParseMessage(MsgBuffer& buf);
    void SetTestMessageCallback(const TestMessageCallback& cb) { cb_ = cb; }
    void SetTestMessageCallback(TestMessageCallback&& cb) { cb_ = std::move(cb); }

private:
    TcpConnectionPtr    connection_;                // 解析之后要给connection
    int                 state_{kTextContextHeader}; // 状态机
    int32_t             message_length_{0};         // 消息长度
    std::string         message_body_;              // 消息体
    TestMessageCallback cb_;
};

} // namespace tmms::net