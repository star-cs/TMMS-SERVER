/*
 * @Author: star-cs
 * @Date: 2025-07-22 21:25:14
 * @LastEditTime: 2025-07-23 11:04:53
 * @FilePath: /TMMS-SERVER/tmms/network/base/inetaddress.h
 * @Description:
 */
/*
 * @Author: star-cs
 * @Date: 2025-07-22 21:25:14
 * @LastEditTime: 2025-07-22 22:19:03
 * @FilePath: /TMMS-SERVER/tmms/network/address/inetaddress.h
 * @Description:
 */
#pragma once

#include <arpa/inet.h>   // 地址转换
#include <bits/socket.h> // socket数据类型
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h> // socket

namespace tmms::net
{
class InetAddress
{
public:
    using ptr = std::shared_ptr<InetAddress>;

    InetAddress(const std::string& ip, uint16_t port, bool bv6 = false);
    InetAddress(const std::string& host, bool bv6 = false);
    InetAddress()  = default;
    ~InetAddress() = default;

    // host = addr:port
    void SetHost(const std::string& host) { GetIpAndPort(host, addr_, port_); }
    void SetAddr(const std::string& addr) { addr_ = addr; }
    void SetPort(uint16_t port) { port_ = std::to_string(port); }
    void SetIsIPv6(bool is_v6) { is_ipv6_ = is_v6; }

    const std::string& IP() const { return addr_; }
    uint32_t           IPv4() const { return IPv4(addr_.c_str()); }
    std::string        ToIpPort() const
    {
        std::stringstream ss;
        ss << addr_ << ":" << port_;
        return ss.str();
    }
    uint16_t Port() { return std::atoi(port_.c_str()); }

    const void GetSockAddr(struct sockaddr* saddr) const;

    static void GetIpAndPort(const std::string& host, std::string& ip, std::string& port);

    // test
    bool IsIpV6() const { return is_ipv6_; }
    bool IsWanIp() const; // 广域网
    bool IsLanIp() const; // 局域网

    bool IsLoopbackIp() const { return addr_ == "127.0.0.1"; } // 回播测试

private:
    uint32_t IPv4(const char* ip) const;

    std::string addr_;
    std::string port_;
    bool        is_ipv6_{false}; // 默认不是ipv6
};
} // namespace tmms::net