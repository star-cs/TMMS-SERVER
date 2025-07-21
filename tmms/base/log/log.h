/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:48:57
 * @LastEditTime: 2025-07-21 21:37:31
 * @FilePath: /TMMS-SERVER/tmms/base/log/log.h
 * @Description:
 */
#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG // 启用DEBUG及以上级别

#include "spdlog/spdlog.h"

namespace tmms::base
{
class Log
{
public:
    Log() {}

    virtual ~Log() {}

    static spdlog::logger* get_core_logger() { return ptr_core_logger_; }

    static spdlog::logger* get_hls_logger() { return ptr_hls_logger_; }

public:
    static void init(bool debug);

private:
    static std::shared_ptr<spdlog::logger> core_logger_;
    static std::shared_ptr<spdlog::logger> hls_logger_;
    static spdlog::logger*                 ptr_core_logger_;
    static spdlog::logger*                 ptr_hls_logger_;
};

#define CORE_TRACE(...) SPDLOG_LOGGER_TRACE(tmms::base::Log::get_core_logger(), __VA_ARGS__)
#define CORE_DEBUG(...) SPDLOG_LOGGER_DEBUG(tmms::base::Log::get_core_logger(), __VA_ARGS__)
#define CORE_INFO(...)  SPDLOG_LOGGER_INFO(tmms::base::Log::get_core_logger(), __VA_ARGS__)
#define CORE_WARN(...)  SPDLOG_LOGGER_WARN(tmms::base::Log::get_core_logger(), __VA_ARGS__)
#define CORE_ERROR(...) SPDLOG_LOGGER_ERROR(tmms::base::Log::get_core_logger(), __VA_ARGS__)
#define CORE_FLUSH()    tmms::base::Log::get_core_logger()->flush()

#define APP_TRACE(APP_PTR, ...) SPDLOG_LOGGER_TRACE(APP_PTR->get_logger(), __VA_ARGS__)
#define APP_DEBUG(APP_PTR, ...) SPDLOG_LOGGER_DEBUG(APP_PTR->get_logger(), __VA_ARGS__)
#define APP_INFO(APP_PTR, ...)  SPDLOG_LOGGER_INFO(APP_PTR->get_logger(), __VA_ARGS__)
#define APP_WARN(APP_PTR, ...)  SPDLOG_LOGGER_WARN(APP_PTR->get_logger(), __VA_ARGS__)
#define APP_ERROR(APP_PTR, ...) SPDLOG_LOGGER_ERROR(APP_PTR->get_logger(), __VA_ARGS__)

#define HLS_TRACE(...) SPDLOG_LOGGER_TRACE(tmms::base::Log::get_hls_logger(), __VA_ARGS__)
#define HLS_DEBUG(...) SPDLOG_LOGGER_DEBUG(tmms::base::Log::get_hls_logger(), __VA_ARGS__)
#define HLS_INFO(...)  SPDLOG_LOGGER_INFO(tmms::base::Log::get_hls_logger(), __VA_ARGS__)
#define HLS_WARN(...)  SPDLOG_LOGGER_WARN(tmms::base::Log::get_hls_logger(), __VA_ARGS__)
#define HLS_ERROR(...) SPDLOG_LOGGER_ERROR(tmms::base::Log::get_hls_logger(), __VA_ARGS__)
#define HLS_FLUSH()    tmms::base::Log::get_hls_logger()->flush()

}; // namespace tmms::base