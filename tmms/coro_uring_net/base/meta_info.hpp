#pragma once

#include "base/config/config.h"
#include <atomic>

namespace tmms::net
{

class context;
class engine;

struct alignas(tmms::config::kCacheLineSize) local_info
{
    context* ctx{nullptr};
    engine*  egn{nullptr};
};

struct global_info
{
    std::atomic<uint32_t> context_id{0};
    std::atomic<uint32_t> engine_id{0};
// TODO: Add more global var
#ifdef ENABLE_MEMORY_ALLOC
    tmms::base::memory_allocator<tmms::config::kMemoryAllocator>* mem_alloc;
#endif
};

inline thread_local local_info linfo;
inline global_info             ginfo;

// init global info
inline auto init_meta_info() noexcept -> void
{
    ginfo.context_id = 0;
    ginfo.engine_id  = 0;
#ifdef ENABLE_MEMORY_ALLOC
    ginfo.mem_alloc = nullptr;
#endif
}

// This function is used to distinguish whether you are currently in a worker thread
inline auto is_in_working_state() noexcept -> bool
{
    return linfo.ctx != nullptr;
}

} // namespace tmms::net