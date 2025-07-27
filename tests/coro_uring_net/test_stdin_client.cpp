/*
 * @Author: star-cs
 * @Date: 2025-07-26 21:54:46
 * @LastEditTime: 2025-07-27 09:32:59
 * @FilePath: /TMMS-SERVER/tests/coro_uring_net/test_stdin_client.cpp
 * @Description:
 */
/*
 * @Author: star-cs
 * @Date: 2025-07-26 21:54:46
 * @LastEditTime: 2025-07-26 22:28:16
 * @FilePath: /TMMS-SERVER/tests/coro_uring_net/test_stdin_client.cpp
 * @Description:
 */
#include "base/log/log.h"
#include "coro_uring_net/coro.hpp"

using namespace tmms::net;

#define BUFFLEN 10240
// nc -lk 8000

task<> echo(int sockfd)
{
    // client等待read的时候，恢复
    char buf[BUFFLEN] = {0};
    int  ret          = 0;
    auto conn         = tcp::tcp_connector(sockfd);

    while (true)
    {
        ret = co_await io::stdin_awaiter(buf, BUFFLEN, 0); // 提交 I/O 任务，等待读取到 命令行 输入
        CORE_DEBUG("receive data from stdin: {}", buf);
        ret = co_await conn.write(buf, ret);
    }
}

task<> client(const char* addr, int port)
{
    auto client = tcp::tcp_client(addr, port);
    int  ret    = 0;
    int  sockfd = 0;
    sockfd      = co_await client.connect();
    assert(sockfd > 0 && "connect error");

    if (sockfd <= 0)
    {
        CORE_DEBUG("connect error");
        co_return;
    }

    // 协程内提交 子协程任务
    submit_to_scheduler(echo(sockfd)); // 当前协程还没释放，不会执行子协程。这里并没有记录父子协程的关系

    char buf[BUFFLEN] = {0};
    auto conn         = tcp::tcp_connector(sockfd);
    while ((ret = co_await conn.read(buf, BUFFLEN)) > 0) // 协程 封装 I/O任务，协程suspend，等待read完成，句柄resume恢复
    {
        CORE_DEBUG("receive data from net: {}", buf);
    }

    ret = co_await conn.close();
    assert(ret == 0);
}

int main(int argc, char const* argv[])
{
    tmms::base::Log::init(true);
    /* code */
    scheduler::init();
    submit_to_scheduler(client("localhost", 8000));

    scheduler::loop();
    return 0;
}
