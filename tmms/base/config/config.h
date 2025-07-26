/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:38:26
 * @LastEditTime: 2025-07-26 10:53:57
 * @FilePath: /TMMS-SERVER/tmms/base/config/config.h
 * @Description:
 */
#pragma once

#include "base/allocator/memory.hpp"
#include "base/singleton.h"

namespace tmms::base
{
class Config : public SingletonPtr<Config>
{
public:
    Config();
    /*
     * @brief 热更新配置
     * @param[in] config_path 配置路径
     */
    bool               reload_config(const std::string& config_path);
    bool               load_config(const std::string& config_path);
    const std::string& get_log_level() const { return log_level_; }

private:
    std::string log_level_ = "info";
};

} // namespace tmms::base

namespace tmms::config
{
// =========================== cpu configuration ============================
// use alignas(kCacheLineSize) to reduce cache invalidation
constexpr size_t kCacheLineSize = 64;

// ====================== memory allocator configuration ====================
// only ENABLE_MEMORY_ALLOC is defined, the memory allocator strategy will be enabled
#define ENABLE_MEMORY_ALLOC

// memory allocator is used to allocate memory for coroutine
constexpr tmms::base::memory_allocator_type kMemoryAllocator = tmms::base::memory_allocator_type::std_allocator;

} // namespace tmms::config