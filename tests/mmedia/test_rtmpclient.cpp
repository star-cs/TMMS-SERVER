#include "base/log/log.h"
#include "mmedia/rtmp/rtmp_client.h"
#include "mmedia/rtmp/rtmp_handler.h"
#include "network/net/eventloopthread.h"
#include <iostream>
#include <thread>

using namespace tmms::base;
using namespace tmms::net;
using namespace tmms::mm;

class RtmpHandlerImpl : public RtmpHandler
{
public:
    // 播放
    bool OnPlay(const TcpConnectionPtr& conn, const std::string& session_name, const std::string& param) override
    {
        return false;
    }
    // 推流
    bool OnPublish(const TcpConnectionPtr& conn, const std::string& session_name, const std::string& param) override
    { 
        return false;
    }
    // 暂停
    bool OnPause(const TcpConnectionPtr& conn, bool pause) override { return false; }
    // 定位(快进等)≈
    void OnSeek(const TcpConnectionPtr& conn, double time) override {}

    void OnNewConnection(const TcpConnectionPtr& conn) override {

    } // 新连接的时候，直播业务就可以处理数据，比如创建用户等
    void OnConnectionDestroy(const TcpConnectionPtr& conn) override {} // 连接断开的时候，业务层可以回收资源，注销用户等
    void OnRecv(const TcpConnectionPtr& conn, const PacketPtr& data) override
    {
        std::cout << "recv type:" << data->PacketType() << " size:" << data->PacketSize() << std::endl;
    } // 多媒体解析出来的数据，传给直播业务
    void OnRecv(const TcpConnectionPtr& conn, PacketPtr&& data) override
    {
        std::cout << "recv type:" << data->PacketType() << " size:" << data->PacketSize() << std::endl;
    }
    void OnActive(const ConnectionPtr& conn) override {} // 新连接回调通知直播业务
};

EventLoopThread eventloop_thread;
std::thread     th;

int main()
{
    Log::init(true);
    eventloop_thread.Run();
    EventLoop* loop = eventloop_thread.Loop();

    if (loop)
    {
        RtmpClient client(loop, new RtmpHandlerImpl());
        client.Play("rtmp://127.0.0.1/live/test");
        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return 0;
}