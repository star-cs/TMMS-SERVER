/*
 * @Author: star-cs
 * @Date: 2025-07-22 21:43:16
 * @LastEditTime: 2025-07-22 23:06:39
 * @FilePath: /TMMS-SERVER/tests/network/test_inetaddress.cpp
 * @Description:
 */
#include "network/base/inetaddress.h"
#include <iostream>
#include <string>

using namespace tmms::net;

int main()
{
    std::string host;
    while (std::cin >> host)
    {
        InetAdress addr(host);
        std::cout << "host:" << host << std::endl
                  << "ip:" << addr.IP() << std::endl
                  << "port:" << addr.Port() << std::endl
                  << "lan:" << addr.IsLanIp() << std::endl
                  << "wan:" << addr.IsWanIp() << std::endl
                  << "loop:" << addr.IsLoopbackIp() << std::endl;
    }
    return 0;
}