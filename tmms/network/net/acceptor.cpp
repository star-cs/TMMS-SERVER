#include "network/net/acceptor.h"
#include "base/log/log.h"
#include "network/base/inetaddress.h"
#include "network/base/socketopt.h"
#include "network/net/event.h"
#include "network/net/eventloop.h"
#include <memory>
#include <sys/socket.h>

namespace tmms::net
{
Acceptor::Acceptor(EventLoop* loop, const InetAddress& addr) : Event(loop), addr_(addr)
{
}

Acceptor::~Acceptor()
{
    Stop();
}

void Acceptor::Start()
{
    loop_->RunInLoop([this]() { Open(); });
}
void Acceptor::Stop()
{
    loop_->DelEvent(std::dynamic_pointer_cast<Acceptor>(shared_from_this()));
}

/// @brief 重载虚函数，epoll调用的时候执行
void Acceptor::OnRead()
{
    while (true)
    {
        InetAddress addr;
        auto        sock = socket_opt_->Accept(&addr);
        if (sock >= 0)
        {
            if (accept_cb_)
            {
                accept_cb_(sock, addr);
            }
        }
        else
        {
            // 因为是非阻塞的socket，accept还没有准备好就返回EAGAIN了，这样的情况不是出错
            if (errno != EINTR && errno != EAGAIN)
            {
                CORE_ERROR("acceptor error.errno : {}", errno);
                OnClose();
            }
            break;
        }
    }
}

void Acceptor::OnClose()
{
    Stop();
    Open();
}

void Acceptor::OnError(const std::string& msg)
{
}

/// @brief 创建socket
void Acceptor::Open()
{
    if (fd_ > 0)
    {
        ::close(fd_);
        fd_ = -1;
    }

    if (addr_.IsIpV6())
    {
        fd_         = SocketOpt::CreateNonblockingTcpSocket(AF_INET6);
        socket_opt_ = std::make_shared<SocketOpt>(fd_, true);
    }
    else
    {
        fd_         = SocketOpt::CreateNonblockingTcpSocket(AF_INET);
        socket_opt_ = std::make_shared<SocketOpt>(fd_, false);
    }
    if(fd_ < 0)
    {
        CORE_ERROR("socket failed.errno: {}" , errno);
        exit(-1);
    }

    loop_->AddEvent(std::dynamic_pointer_cast<Acceptor>(shared_from_this()));
    socket_opt_->SetReusePort(true);
    socket_opt_->SetReuseAddr(true);
    socket_opt_->BindAddress(addr_);
    socket_opt_->Listen();
}

} // namespace tmms::net