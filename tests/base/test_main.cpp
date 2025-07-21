/*
 * @Author: star-cs
 * @Date: 2025-07-21 14:10:33
 * @LastEditTime: 2025-07-21 21:33:56
 * @FilePath: /TMMS-SERVER/tests/base/test_main.cpp
 * @Description:
 */

#include "base/config/config.h"
#include "base/env.h"
#include "base/log/log.h"
#include "base/utils/utils.h"
#include <filesystem>
#include <spdlog/spdlog.h>

using namespace tmms::base;

void init_log(bool is_debug_mode)
{
    Log::init(is_debug_mode);
    CORE_TRACE("Welcome to tmms!");
    CORE_DEBUG("Welcome to tmms!");
    CORE_INFO("Welcome to tmms!");
    CORE_WARN("Welcome to tmms!");
    CORE_ERROR("Welcome to tmms!");
}

int main(int argc, char* argv[])
{
    Env::GetInstance()->parse(argc, argv);

    auto level       = Env::GetInstance()->getOption("log_level", "info");
    auto config_path = Env::GetInstance()->getOption("config", std::filesystem::path(Utils::get_bin_path()) / "config");

    try
    {
        if (!Config::GetInstance()->load_config(config_path))
        {
            spdlog::error("failed load config:{}, program exit!", config_path);
            return -3;
        }
    }
    catch (std::exception& exp)
    {
        CORE_ERROR("load config failed, err:{}", exp.what());
        return -4;
    }
    // 按照 yaml来
    init_log(Config::GetInstance()->get_log_level() == "debug");
    return 0;
}