/*
 * @Author: star-cs
 * @Date: 2025-07-25 10:21:44
 * @LastEditTime: 2025-07-25 10:25:24
 * @FilePath: /TMMS-SERVER/tmms/network/base/dns_service.h
 * @Description:
 */
#pragma once
#include "base/noncopyable.h"
#include "base/singleton.h"
#include "network/base/inetaddress.h"
#include <thread>
#include <unordered_map>
#include <vector>

namespace tmms::net
{
using namespace tmms::base;

class DnsService : public NonCopyable, public Singleton<DnsService>
{
public:
    DnsService()  = default;
    ~DnsService() = default;

    void                          AddHost(const std::string& host);
    InetAddress::ptr              GetHostAddress(const std::string& host, int index);
    std::vector<InetAddress::ptr> GetHostAddress(const std::string& host);
    void                          UpdateHost(const std::string& host, std::vector<InetAddress::ptr>& list);
    std::unordered_map<std::string, std::vector<InetAddress::ptr>> GetHosts();
    void        SetDnsServiceParam(int32_t interval, int32_t sleep, int32_t retry);
    void        Start();
    void        Stop();
    void        OnWork();
    static void GetHostInfo(const std::string& host, std::vector<InetAddress::ptr>& list);

private:
    std::thread thread_;
    bool        running_{false};
    std::mutex  lock_;
    // 域名，地址对
    std::unordered_map<std::string, std::vector<InetAddress::ptr>> hosts_info_;
    int32_t                                                        retry_{3};
    int32_t                                                        sleep_{200};           // ms
    int32_t                                                        interval_{180 * 1000}; // ms，6mintue
};

} // namespace tmms::net