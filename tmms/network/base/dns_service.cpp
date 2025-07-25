/*
 * @Author: star-cs
 * @Date: 2025-07-25 10:24:41
 * @LastEditTime: 2025-07-25 10:31:17
 * @FilePath: /TMMS-SERVER/tmms/network/base/dns_service.cpp
 * @Description:
 */
#include "network/base/dns_service.h"
#include "network/base/inetaddress.h"
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace tmms::net
{
namespace
{
static InetAddress::ptr inet_address_null;
}

/// @brief 添加host和ip地址的映射对，如果有就直接返回，没有就插入一个空的vector
/// @param host  域名
void DnsService::AddHost(const std::string& host)
{
    std::lock_guard<std::mutex> lk(lock_);
    auto                        iter = hosts_info_.find(host);
    if (iter != hosts_info_.end())
    {
        return;
    }

    hosts_info_[host] = std::vector<InetAddress::ptr>();
}

/// @brief 获取域名对应的ip地址类
/// @param host
/// @param index
/// @return 对应的ip地址类，如果没有找到，返回一个空的智能指针
InetAddress::ptr DnsService::GetHostAddress(const std::string& host, int index)
{
    std::lock_guard<std::mutex> lk(lock_);

    auto iter = hosts_info_.find(host);
    if (iter != hosts_info_.end())
    {
        auto list = iter->second;
        if (list.size() > 0)
        {
            return list[index % list.size()];
        }
    }

    return inet_address_null;
}

/// @brief 一个域名可能对应多个ip地址，取所有的对应ip地址
/// @param host
/// @return 一个ip地址的数组
std::vector<InetAddress::ptr> DnsService::GetHostAddress(const std::string& host)
{
    std::lock_guard<std::mutex> lk(lock_);

    auto iter = hosts_info_.find(host);
    if (iter != hosts_info_.end())
    {
        return iter->second;
    }

    return std::vector<InetAddress::ptr>();
}

/// @brief 替换整个域名列表
/// @param host
/// @param list
void DnsService::UpdateHost(const std::string& host, std::vector<InetAddress::ptr>& list)
{
    std::lock_guard<std::mutex> lk(lock_);
    hosts_info_[host].swap(list);
}

/// @brief 返回整个域名表
/// @return 返回整个域名表
std::unordered_map<std::string, std::vector<InetAddress::ptr>> DnsService::GetHosts()
{
    std::lock_guard<std::mutex> lk(lock_);
    return hosts_info_;
}

/// @brief 设置dns的参数，
/// @param interval
/// @param sleep
/// @param retry
void DnsService::SetDnsServiceParam(int32_t interval, int32_t sleep, int32_t retry)
{
    interval_ = interval;
    sleep_    = sleep;
    retry_    = retry;
}

void DnsService::Start()
{
    running_ = true;
    thread_  = std::thread(std::bind(&DnsService::OnWork, this));
}

void DnsService::Stop()
{
    running_ = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void DnsService::OnWork()
{
    while (running_)
    {
        auto hosts_info_ = GetHosts();
        for (auto& host : hosts_info_)
        {
            for (int i = 0; i < retry_; i++)
            {
                std::vector<InetAddress::ptr> list;
                GetHostInfo(host.first, list);
                if (list.size() > 0)
                {
                    UpdateHost(host.first, list);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_));
    }
}

void DnsService::GetHostInfo(const std::string& host, std::vector<InetAddress::ptr>& list)
{
}

} // namespace tmms::net