#include "base/log/log.h"
#include "network/base/dns_service.h"
#include "network/base/inetaddress.h"
#include <iostream>
#include <vector>

using namespace tmms::net;
using namespace tmms::base;

int main()
{
    Log::init(true);

    std::vector<InetAddress::ptr> list;
    DnsService::GetInstance()->AddHost("www.baidu.com");
    DnsService::GetInstance()->Start();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    list = DnsService::GetInstance()->GetHostAddress("www.baidu.com");
    for (auto& i : list)
    {
        std::cout << "ip: " << i->IP() << std::endl;
    }
    return 0;
}