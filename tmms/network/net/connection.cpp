#include "network/net/connection.h"
#include "network/net/eventloop.h"
#include <memory>

namespace tmms::net
{
Connection::Connection(EventLoop* loop, int fd, const InetAddress& localAddr, const InetAddress& peerAddr)
    : Event(loop, fd),
      local_addr_(localAddr),
      peer_addr_(peerAddr)
{
}

void Connection::Active()
{
    if (!active_.load())
    {
        loop_->RunInLoop([this](){  // 放在loop中进行回调执行，OnRead()中执行这个回调
            active_.store(true);
            if(active_cb_){
                active_cb_(std::dynamic_pointer_cast<Connection>(shared_from_this()));
            }
        });
    }
}

void Connection::Deactive()
{
    active_.store(false);
}
} // namespace tmms::net