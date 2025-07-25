#include "network/net/tcpclient.h"
#include "base/log/log.h"
#include "network/base/socketopt.h"
#include "network/net/eventloop.h"
#include "network/net/tcpconnection.h"

namespace tmms::net
{
TcpClient::TcpClient(EventLoop* loop, const InetAddress& server)
    : TcpConnection(loop, -1, InetAddress(), server),
      server_addr_(server)
{
}

TcpClient::~TcpClient()
{
    OnClose();
}

void TcpClient::Connect()
{
    loop_->RunInLoop([this]() { ConnectInLoop(); });
}

/// @brief 判断现在的链接状态，如果已连接，直接调用父类的OnRead读取数据到buffer
void TcpClient::OnRead()
{
    if (status_ == kTcpConStatusConnecting)
    {
        if (CheckError())
        {
            CORE_ERROR("connect to server:{} error:{}", server_addr_.ToIpPort(), errno);
            OnClose();
            return;
        }
        UpdateConnectionStatus(); // 连接的状态变化很快，已经在连接中，并且没有错误发生，说明下一刻就是链接成功了
        return;
    }
    else if (status_ == kTcpConStatusConnected)
    {
        TcpConnection::OnRead();
    }
}

void TcpClient::OnWrite()
{
    if (status_ == kTcpConStatusConnecting)
    {
        if (CheckError())
        {
            CORE_ERROR("connect to server:{} error:{}", server_addr_.ToIpPort(), errno);
            OnClose();
            return;
        }
        UpdateConnectionStatus(); // 连接的状态变化很快，已经在连接中，并且没有错误发生，说明下一刻就是链接成功了
        return;
    }
    else if (status_ == kTcpConStatusConnected)
    {
        TcpConnection::OnWrite();
    }
}
/// @brief 关闭正在连接或已连接的状态，不是这两个状态没法关闭
void TcpClient::OnClose()
{
    if (status_ == kTcpConStatusConnecting || status_ == kTcpConStatusConnected)
    {
        loop_->DelEvent(std::dynamic_pointer_cast<TcpClient>(shared_from_this()));
    }

    status_ = kTcpConStatusDisConnected;
    TcpConnection::OnClose();
}

/// @brief 发送一个buffer的list，通过TcoConnection发送即可
/// @param list
void TcpClient::Send(std::list<BufferNode::ptr>& list)
{
    if (status_ == kTcpConStatusConnected)
    {
        TcpConnection::Send(list);
    }
}

void TcpClient::Send(const char* buf, size_t size)
{
    if (status_ == kTcpConStatusConnected)
    {
        TcpConnection::Send(buf, size);
    }
}

// @brief 进行connection客户端连接，由Connection函数调用，在同一个loop中
void TcpClient::ConnectInLoop()
{
    loop_->AssertInLoopThread(); // 一定要在同一个线程loop中执行
    fd_ = SocketOpt::CreateNonblockingTcpSocket(AF_INET);
    if (fd_ < 0)
    {
        OnClose();
        return;
    }

    status_ = kTcpConStatusConnecting; // 连接中
    // 进行shared_from_this的时候，必须本身就是个智能指针类型，使用类的时候一定要用智能指针

    loop_->AddEvent(std::dynamic_pointer_cast<TcpClient>(shared_from_this()));
    EnableWriting(true);
    SocketOpt opt(fd_);
    auto      ret = opt.Connect(server_addr_); // connection
    if (ret == 0)                              // 连接成功
    {
        UpdateConnectionStatus();
        return;
    }
    else if (ret == -1) // 失败，并且errno 不是EINPROGRESS（连接处理中）
    {
        if (errno != EINPROGRESS)
        {
            CORE_ERROR("connect to server:{} error:{}", server_addr_.ToIpPort(), errno);
            OnClose();
            return;
        }
    }
}

/// @brief 更新连接状态，调用回调传出自身
void TcpClient::UpdateConnectionStatus()
{
    status_ = kTcpConStatusConnected;
    if (connected_cb_)
    {
        connected_cb_(std::dynamic_pointer_cast<TcpClient>(shared_from_this()), true);
    }
}

/// @brief 通过getsockopt获取socket的错误码
/// @return 错误码为0，是否无错误
bool TcpClient::CheckError()
{
    int       error = 0;
    socklen_t len   = sizeof(error);
    ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len);
    return error != 0;
}

} // namespace tmms::net