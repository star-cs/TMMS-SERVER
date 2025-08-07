/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:49:31
 * @LastEditTime: 2025-07-28 12:07:52
 * @FilePath: /TMMS-SERVER/tmms/base/log/log.cpp
 * @Description:
 */
#include <filesystem>

#include "log.h"
#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"

#include "base/utils/utils.h"

using namespace tmms::base;
std::shared_ptr<spdlog::logger> Log::core_logger_;
std::shared_ptr<spdlog::logger> Log::live_logger_;
std::shared_ptr<spdlog::logger> Log::rtmp_logger_;

spdlog::logger* Log::ptr_core_logger_ = nullptr;
spdlog::logger* Log::ptr_live_logger_ = nullptr;
spdlog::logger* Log::ptr_rtmp_logger_ = nullptr;

void Log::init(bool debug)
{
    if (ptr_core_logger_ != nullptr && ptr_live_logger_ != nullptr && ptr_rtmp_logger_ != nullptr)
    {
        return;
    }
    
    if (debug)
    {
        spdlog::set_level(spdlog::level::trace);

        core_logger_     = spdlog::stdout_color_mt("core");
        live_logger_     = spdlog::stdout_color_mt("live");
        rtmp_logger_     = spdlog::stdout_color_mt("rtmp");
        ptr_core_logger_ = core_logger_.get();
        ptr_live_logger_ = live_logger_.get();
        ptr_rtmp_logger_ = rtmp_logger_.get();

        auto debug_format = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%@] %v";

        core_logger_->set_pattern(debug_format);
        live_logger_->set_pattern(debug_format);
        ptr_rtmp_logger_->set_pattern(debug_format);

        core_logger_->flush_on(spdlog::level::debug);
        live_logger_->flush_on(spdlog::level::debug);
        rtmp_logger_->flush_on(spdlog::level::debug);
    }
    else
    {
        spdlog::set_level(spdlog::level::info);

        std::filesystem::create_directories(Utils::get_bin_path() + "/logs");
        core_logger_ = spdlog::daily_logger_mt("core", Utils::get_bin_path() + "/logs/core.log", 00, 00);
        live_logger_ = spdlog::daily_logger_mt("hls", Utils::get_bin_path() + "/logs/hls.log", 00, 00);
        rtmp_logger_ = spdlog::daily_logger_mt("rtmp", Utils::get_bin_path() + "/logs/rtmp.log");

        ptr_core_logger_ = core_logger_.get();
        ptr_live_logger_ = live_logger_.get();
        ptr_rtmp_logger_ = rtmp_logger_.get();

        core_logger_->flush_on(spdlog::level::info);
        live_logger_->flush_on(spdlog::level::info);
        rtmp_logger_->flush_on(spdlog::level::info);
    }
}