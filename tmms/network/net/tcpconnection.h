#pragma once

#include "network/base/msgbuffer.h"
#include "network/net/connection.h"
#include <list>
#include <memory>
namespace tmms::net
{
class TimeoutEntry;
class TcpConnection;
using TcpConnectionPtr        = std::shared_ptr<TcpConnection>;
using CloseConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback         = std::function<void(const TcpConnectionPtr&, MsgBuffer&)>;
using WriteCompleteCallback   = std::function<void(const TcpConnectionPtr&)>;
using TimeoutCallback         = std::function<void(const TcpConnectionPtr&)>;

struct BufferNode // buffer的存储节点
{
    using ptr = std::shared_ptr<BufferNode>;

    BufferNode(void* buf, size_t s) : addr(buf), size(s) {}
    void*  addr{nullptr};
    size_t size{0};
};

using BufferNodePtr = std::shared_ptr<BufferNode>;

class TcpConnection : public Connection
{
public:
    using ptr = std::shared_ptr<TcpConnection>;
    TcpConnection(EventLoop* loop, int socketfd, const InetAddress& localAddr, const InetAddress& peerAddr);
    virtual ~TcpConnection();

    ////// 事件
    // 关闭事件
    void SetCloseCallback(const CloseConnectionCallback& cb) { close_cb_ = cb; }
    void SetCloseCallback(CloseConnectionCallback&& cb) { close_cb_ = std::move(cb); }

    void OnClose() override;
    void ForceClose() override;

    // 读事件
    void OnRead() override;
    void SetRecvMsgCallback(const MessageCallback& cb) { message_cb_ = cb; }
    void SetRecvMsgCallback(MessageCallback&& cb) { message_cb_ = std::move(cb); }

    // 写事件
    void OnWrite() override;
    void SetWriteCompleteCallback(const WriteCompleteCallback& cb) { write_complete_cb_ = cb; }
    void SetWriteCompleteCallback(WriteCompleteCallback&& cb) { write_complete_cb_ = std::move(cb); }

    void Send(std::list<BufferNodePtr>& list); // 传多个buffer
    void Send(const char* buf, size_t size);   // 发送单个buffer
                                               // 超时事件
    void OnTimeout();

    // 伪闭包，传入智能指针，timeout之间内，保证生命周期
    void SetTimeoutCallback(int timeout, const TimeoutCallback& cb);
    void SetTimeoutCallback(int timeout, TimeoutCallback&& cb);

    void EnableCheckIdleTimeout(int32_t max_time);

    void OnError(const std::string& msg) override;

private:
    void SendInLoop(const char* buf, size_t size);
    void SendInLoop(std::list<BufferNodePtr>& list);

    void ExtendLife(); // 延长timeout的生命周期

    bool                    closed_{false};
    CloseConnectionCallback close_cb_;
    MsgBuffer               message_buffer_; // 读数据的缓存
    MessageCallback         message_cb_;     // 参数表示，哪个连接，使用的哪个buffer

    std::vector<struct iovec> io_vec_list_; // 发送数据的缓存
    WriteCompleteCallback     write_complete_cb_;

    std::weak_ptr<TimeoutEntry> timeout_entry_;     // 弱引用，防止循环引用
    int32_t                     max_idle_time_{30}; // 可以通过配置文件配置
};

class TimeoutEntry // 超时任务监听，智能指针引用结束的时候
{
public:
    TimeoutEntry(const TcpConnectionPtr& c) : conn(c) {}
    ~TimeoutEntry()
    {
        auto c = conn.lock(); // 弱引用升级，实现超时回调
        if (c)
        {
            c->OnTimeout();
        }
    }

private:
    std::weak_ptr<TcpConnection> conn;
};

} // namespace tmms::net