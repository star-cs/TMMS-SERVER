/*
 * @Author: star-cs
 * @Date: 2025-07-26 10:37:44
 * @LastEditTime: 2025-07-26 11:29:27
 * @FilePath: /TMMS-SERVER/tmms/meta_info.hpp
 * @Description:
 */
#include "base/config/config.h"
#include <atomic>

namespace tmms::meta
{

class context;
class engine;

struct alignas(tmms::config::kCacheLineSize) local_info
{
    context* ctx{nullptr};
    engine*  egn{nullptr};
    // TODO: Add more local var
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
inline global_info ginfo;

} // namespace tmms::meta