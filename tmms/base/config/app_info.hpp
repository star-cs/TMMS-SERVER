#pragma once
#include "base/log/log.h"
#include <cstdint>
#include <string>
#include <yaml-cpp/yaml.h>
namespace tmms::base
{

class DomainInfo;
class AppInfo
{
public:
    explicit AppInfo(DomainInfo& d) : domain_info(d) {}
    ~AppInfo() = default;

    bool ParseAppInfo(const YAML::Node& root)
    {
        auto name              = root["name"];
        auto max_buf           = root["max_buffer"];
        auto hls_sup           = root["hls_support"];
        auto flv_sup           = root["flv_support"];
        auto rtmp_sup          = root["rtmp_support"];
        auto content_lat       = root["content_latency"];
        auto stream_idle_ti    = root["stream_idle_time"];
        auto stream_timeout_ti = root["stream_timeout_time"];
        if (!name.IsDefined() || !max_buf.IsDefined() || !hls_sup.IsDefined() || !flv_sup.IsDefined() ||
            !rtmp_sup.IsDefined() || !content_lat.IsDefined())
        {
            return false;
        }
        app_name            = name.as<std::string>();
        max_buffer          = max_buf.as<uint32_t>();
        hls_support         = hls_sup.as<bool>();
        flv_support         = flv_sup.as<bool>();
        rtmp_support        = rtmp_sup.as<bool>();
        content_latency     = content_lat.as<uint32_t>() * 1000; // 毫秒
        stream_idle_time    = stream_idle_ti.as<uint32_t>();
        stream_timeout_time = stream_timeout_ti.as<uint32_t>();

        CORE_INFO(
            "app_name:{}, max_buffer:{}, flv_support:{}, hls_support:{}, rtmp_support:{}, content_latency:{}, stream_idle_time:{}, stream_timeout_time:{}",
            app_name,
            max_buffer,
            flv_support,
            hls_support,
            rtmp_support,
            content_latency,
            stream_idle_time,
            stream_timeout_time);

        return true;
    }

public:
    DomainInfo& domain_info; // 引用数据成员一定要在构造函数的序列化参数初始化
    std::string app_name;
    uint32_t    max_buffer{1000};    // 默认1000帧
    bool        rtmp_support{false}; // 默认不支持rtmp
    bool        flv_support{false};
    bool        hls_support{false};
    uint32_t    content_latency{3 * 1000};      // 直播延时，单位ms
    uint32_t    stream_idle_time{30 * 1000};    // 流间隔最大时长
    uint32_t    stream_timeout_time{30 * 1000}; // 流超时时间
};

using AppInfoPtr = std::shared_ptr<AppInfo>;
} // namespace tmms::base