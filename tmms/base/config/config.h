/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:38:26
 * @LastEditTime: 2025-07-26 22:21:57
 * @FilePath: /TMMS-SERVER/tmms/base/config/config.h
 * @Description:
 */
#pragma once

#include "base/allocator/memory.hpp"
#include "base/singleton.h"
#include "base/types.hpp"

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

// ===================== execute engine configuration =======================
using ctx_id = uint32_t;
// engine task queue length, at least >= 4096,
// if submit task to a full task queue:
//    case in working thread: exec task directly
//    case not in working thread: report error and ignore this task
constexpr size_t kQueCap = 16384;

// scheduler dispacher strategy
constexpr tmms::base::dispatch_strategy kDispatchStrategy = tmms::base::dispatch_strategy::round_robin;

// ========================== uring configuration ===========================
// io_uring queue length
constexpr unsigned int kEntryLength = 10240;

// 将 kEnableFixfd 设置为 true，可通过 IOSQE_FIXED_FILE 标志优化 io_uring 性能
constexpr bool kEnableFixfd = false;

// 在 io_uring 中注册的固定文件描述符缓冲区长度，
// 若你的应用程序需通过单个文件描述符发起大量 I/O 操作，仅需增大该参数值即可。
constexpr unsigned int kFixFdArraySize = 8;


// =========================== tcp configuration ============================
constexpr int kDefaultPort = 8000;
constexpr int kBacklog     = 5;

} // namespace tmms::config