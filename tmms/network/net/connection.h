/*
 * @Author: star-cs
 * @Date: 2025-07-23 15:19:08
 * @LastEditTime: 2025-07-27 10:17:28
 * @FilePath: /TMMS-SERVER/tmms/network/net/connection.h
 * @Description:
 */
#pragma once

#include "network/base/inetaddress.h"
#include "network/net/event.h"
#include <atomic>
#include <functional>
#include <memory>
namespace tmms::net
{
enum
{
    kNormalContext = 0,
    kRtmpContext, // rtmp
    kHttpContext, // http
    kUserContext,
    kFlvContext // flv
};

class Connection;
using ConnectionPtr  = std::shared_ptr<Connection>;
using ContextPtr     = std::shared_ptr<void>;
using ActiveCallback = std::function<void(const ConnectionPtr&)>; // 连接时候触发的回调函数，返回连接，进行操作

class Connection : public Event // connection类作为连接的基类
{
public:
    using ptr = std::shared_ptr<Connection>;

    Connection(EventLoop* loop, int fd, const InetAddress& localAddr, const InetAddress& peerAddr);
    virtual ~Connection() = default;

    void               SetLoaclAddr(const InetAddress& local) { local_addr_ = local; }
    void               SetPeerAddr(const InetAddress& peer) { peer_addr_ = peer; }
    const InetAddress& LocalAddr() const { return local_addr_; }
    const InetAddress& PeerAddr() const { return peer_addr_; }

    //////// 上下文相关
    void SetContext(int type, const std::shared_ptr<void>& context) { contexts_[type] = context; } // 设置上下文
    void SetContext(int type, const std::shared_ptr<void>&& context)                               // 设置上下文
    {
        contexts_[type] = std::move(context);
    }

    template<typename T>
    std::shared_ptr<T> GetContext(int type) const // 获取上下文
    {
        auto iter = contexts_.find(type);
        if (iter == contexts_.end())
        {
            return std::shared_ptr<T>();
        }
        return std::static_pointer_cast<T>(iter->second);
    }

    void ClearContext(int type) { contexts_[type].reset(); }
    void ClearContext() { contexts_.clear(); }
    //////// 激活函数相关
    void SetActiveCallback(const ActiveCallback& cb) { active_cb_ = cb; }
    void SetActiveCallback(const ActiveCallback&& cb) { active_cb_ = std::move(cb); }
    void Active();
    void Deactive();

    //////// 关闭时间   子类实现，tcp和udp都要继承连接，关闭的时候做的事不一样
    virtual void ForceClose() = 0;

protected:
    InetAddress local_addr_;
    InetAddress peer_addr_;

private:
    std::unordered_map<int, ContextPtr> contexts_; // 连接上下文管理
    ActiveCallback                      active_cb_;
    std::atomic<bool>                   active_; // 设置激活状态,不同线程调用
};

} // namespace tmms::net