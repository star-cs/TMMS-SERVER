/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:48:15
 * @LastEditTime: 2025-07-21 21:25:06
 * @FilePath: /TMMS-SERVER/tmms/base/config/config.cpp
 * @Description: 
 */
#include "base/config/config.h"
#include "yaml-cpp/yaml.h"
#include <filesystem>
#include <future>
#include <string>

namespace tmms::base
{
Config::Config()
{
}

bool Config::reload_config(const std::string& config_path)
{
    std::shared_ptr<Config> new_config = std::make_shared<Config>();
    if(!new_config->load_config(config_path)){
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
    if(log_level.IsDefined()){
        log_level_ = log_level.as<std::string>();
    }

    return true;
}

} // namespace tmms::base