#pragma once
#include "base/config/app_info.hpp"
#include "base/log/log.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace tmms::base
{

class DomainInfo
{
public:
    DomainInfo(/* args */) = default;
    ~DomainInfo()          = default;

public:
    const std::string& DomainName() const { return name_; } // 返回域名
    const std::string& Type() const { return type_; }
    AppInfoPtr         GetAppInfo(const std::string& app_name) { return appinfos_[app_name]; }

    bool ParseDomainInfo(const std::string& file)
    {
        CORE_DEBUG("domain file : {}", file);

        YAML::Node config;
        try
        {
            config = YAML::LoadFile(file);
        }
        catch (YAML::ParserException& ex)
        {
            return false;
        }
        catch (YAML::BadFile& ex)
        {
            return false;
        }

        auto domain = config["domain"];
        if (!domain.IsDefined())
        {
            return false;
        }

        auto name = domain["name"];
        auto type = domain["type"];
        auto app  = domain["app"];
        if (!name.IsDefined() || !type.IsDefined() || !app.IsDefined())
        {
            return false;
        }

        name_ = name.as<std::string>();
        type_ = type.as<std::string>();

        for (const auto& app_info : app)
        {
            AppInfoPtr app_info_ptr = std::make_shared<AppInfo>(*this);

            auto ret = app_info_ptr->ParseAppInfo(app_info);
            if (!ret)
            {
                return false;
            }

            appinfos_.insert(std::make_pair(app_info_ptr->app_name, app_info_ptr));
        }
        CORE_INFO("domain_name: {}, type: {}, app_num: {}", name_, type_, appinfos_.size());

        return true;
    }

private:
    std::string                                 name_; // domain name
    std::string                                 type_;
    std::mutex                                  lock_;
    std::unordered_map<std::string, AppInfoPtr> appinfos_;
};
} // namespace tmms::base