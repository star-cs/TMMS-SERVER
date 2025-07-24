
#include "network/base/inetaddress.h"
#include "network/net/acceptor.h"
#include "network/net/eventloop.h"
#include "network/net/eventloopthread.h"
#include "base/log/log.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace tmms::base;
using namespace tmms::net;

EventLoopThread eventloop_thread;
std::thread     th;

void test()
{
    eventloop_thread.Run();
    EventLoop*    loop = eventloop_thread.Loop();
    InetAddress   addr("127.0.0.1:34444");
    Acceptor::ptr acceptor = std::make_shared<Acceptor>(loop, addr);
    acceptor->SetAcceptCallback([](int fd, const InetAddress& addr)
                                { std::cout << "host : " << addr.ToIpPort() << std::endl; });
    acceptor->Start();

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    Log::init(true);
    test();
    return 0;
}