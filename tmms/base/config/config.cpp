/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:48:15
 * @LastEditTime: 2025-07-21 21:25:06
 * @FilePath: /TMMS-SERVER/tmms/base/config/config.cpp
 * @Description:
 */
#include "base/config/config.h"
#include "base/config/domin_info.hpp"
#include "base/log/log.h"
#include "base/utils/utils.h"
#include <filesystem>
#include <memory>
#include <string>
#include <yaml-cpp/yaml.h>

#include <dirent.h>   // 目录头文件
#include <sys/stat.h> // 文件属性
#include <unistd.h>

namespace tmms::base
{

namespace
{
static ServiceInfoPtr service_info_nullptr;
}

Config::Config()
{
}

bool Config::reload_config(const std::string& config_path)
{
    std::shared_ptr<Config> new_config = std::make_shared<Config>();
    if (!new_config->load_config(config_path))
    {
        return false;
    }
    return true;
}

bool Config::load_config(const std::string& config_path)
{
    YAML::Node config;
    try
    {
        config = YAML::LoadFile(std::filesystem::path(config_path) / "tmms.yaml");
    }
    catch (YAML::ParserException& ex)
    {
        return false;
    }
    catch (YAML::BadFile& ex)
    {
        return false;
    }
    YAML::Node log_level = config["log_level"];
    if (log_level.IsDefined())
    {
        log_level_ = log_level.as<std::string>();
        Log::init(log_level_ == "debug");
    }

    YAML::Node service_info = config["services"];
    if (service_info.IsDefined())
    {
        if (!ParseServiceInfo(service_info))
        {
            return false;
        }
    }

    YAML::Node directory = config["directory"];
    if (directory.IsDefined())
    {
        ParseDirectory(directory);
    }

    return true;
}

const ServiceInfoPtr& Config::GetServiceInfo(const std::string& portocol, const std::string& transport)
{
    for (auto& s : services_)
    {
        if (s->portocol == portocol && transport == s->transport)
        {
            return s;
        }
    }
    return service_info_nullptr;
}

bool Config::ParseServiceInfo(const YAML::Node& serviceObj)
{
    // todo 初始化 services_
    if (!serviceObj.IsSequence())
    {
        return false;
    }
    for (const auto& service : serviceObj)
    {
        auto addr      = service["addr"];
        auto port      = service["port"];
        auto portocol  = service["portocol"];
        auto transport = service["transport"];
        if (!addr.IsDefined() || !port.IsDefined() || !portocol.IsDefined() || !transport.IsDefined())
        {
            return false;
        }

        ServiceInfoPtr serviceptr = std::make_shared<ServiceInfo>();
        serviceptr->addr          = addr.as<std::string>();
        serviceptr->port          = port.as<uint16_t>();
        serviceptr->portocol      = portocol.as<std::string>();
        serviceptr->transport     = transport.as<std::string>();

        CORE_DEBUG(
            "serverInfo addr:{}, port:{}, portocol:{}, transport:{}",
            serviceptr->addr,
            serviceptr->port,
            serviceptr->portocol,
            serviceptr->transport);

        services_.push_back(serviceptr);
    }

    return true;
}

// 直播业务层配置相关
AppInfoPtr Config::GetAppInfo(const std::string& domain, const std::string& app)
{
    return domaininfos_[domain]->GetAppInfo(app);
}

DomainInfoPtr Config::GetDomainInfo(const std::string& domain)
{
    return domaininfos_[domain];
}

/// @brief 解析直播服务配置文件的目录，列表中可以是文件或者目录
/// @param root
/// @return
bool Config::ParseDirectory(const YAML::Node& root)
{
    if (root.IsNull() || !root.IsSequence())
    {
        return false;
    }
    for (const auto& node : root)
    {
        std::string path = std::filesystem::path(Utils::get_bin_path()) / node["path"].as<std::string>();
        struct stat st;
        CORE_TRACE("ParseDirectory path:{}", path);
        auto ret = stat(path.c_str(), &st);
        CORE_TRACE("ret:{} errno:{}", ret, errno);
        if (ret != -1)
        {
            // S_IFMT屏蔽与文件类型无关的标志位，剩下的判断文件类型
            if ((st.st_mode & S_IFMT) == S_IFDIR)
            {
                ParseDomainPath(path);
            }
            else if ((st.st_mode & S_IFMT) == S_IFREG) // 普通文件
            {
                ParseDomainFile(path);
            }
        }
    }
    return true;
}

/// @brief 递归处理目录下的配置文件
/// @param path
/// @return
bool Config::ParseDomainPath(const std::string& path)
{
    DIR*           dp = nullptr;
    struct dirent* pp = nullptr;

    CORE_DEBUG("parse domain path:{}", path);
    dp = opendir(path.c_str()); // 打开目录
    if (!dp)                    // 打开失败
    {
        return false;
    }

    // 循环遍历目录下的文件
    while (true)
    {
        pp = readdir(dp);  // 从目录中读一个文件
        if (pp == nullptr) // 为空表示读完了
        {
            break;
        }
        if (pp->d_name[0] == '.') // 处理隐藏文件不处理
        {
            continue;
        }
        if (pp->d_type == DT_REG) // 是文件类型
        {
            if (path.at(path.size() - 1) != '/') // 要加前面的路径
            {
                ParseDomainFile(path + "/" + pp->d_name);
            }
            else
            {
                ParseDomainFile(path + pp->d_name);
            }
        }
    }

    closedir(dp);
    return true;
}

bool Config::ParseDomainFile(const std::string& file)
{
    CORE_DEBUG("parse domain file: {}", file);
    DomainInfoPtr d   = std::make_shared<DomainInfo>();
    auto          ret = d->ParseDomainInfo(file);
    if (ret)
    {
        std::lock_guard<std::mutex> lk(lock_);
        // 查找一下，找到了要删除，因为新的要覆盖
        auto iter = domaininfos_.find(d->DomainName());
        if (iter != domaininfos_.end())
        {
            domaininfos_.erase(iter);
        }
        domaininfos_.emplace(d->DomainName(), d); // 添加新的
    }
    return true;
}

void Config::SetDomainInfo(const std::string& domain, DomainInfoPtr& p)
{
}
void Config::SetAppInfo(const std::string& domain, const std::string& app)
{
}

} // namespace tmms::base