/*
 * @Author: star-cs
 * @Date: 2025-07-27 13:45:25
 * @LastEditTime: 2025-07-28 11:00:32
 * @FilePath: /TMMS-SERVER/tmms/mmedia/base/mmediahander.h
 * @Description:
 */
#pragma once

#include "base/noncopyable.h"
#include "mmedia/base/packet.h"
#include "network/net/tcpconnection.h"

namespace tmms::mm
{
using namespace tmms::base;
using namespace tmms::net;

class MMediaHandler : public NonCopyable
{
public:
    virtual void
        OnNewConnection(const TcpConnectionPtr& conn) = 0; // 新连接的时候，直播业务就可以处理数据，比如创建用户等
    virtual void
        OnConnectionDestroy(const TcpConnectionPtr& conn) = 0; // 连接断开的时候，业务层可以回收资源，注销用户等
    virtual void
        OnRecv(const TcpConnectionPtr& conn, const Packet::ptr& data)     = 0; // 多媒体解析出来的数据，传给直播业务
    virtual void OnRecv(const TcpConnectionPtr& conn, Packet::ptr&& data) = 0;
    virtual void OnActive(const ConnectionPtr& conn)                      = 0; // 新连接回调通知直播业务
};
} // namespace tmms::mm