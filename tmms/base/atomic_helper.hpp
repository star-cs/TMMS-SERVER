#pragma once
#include <atomic>

#include "base/config/config.h"

namespace tmms::base
{
/**
 * atomic_ref_wrapper 结构体按缓存行对齐（如 64 字节），确保不同线程的标志不在同一缓存行，避免多核竞争导致的性能下降
 * @brief std::vector<std::atomic<T>> will cause compile error, so use atomic_ref_wrapper
 * @tparam T
 */
template<typename T>
struct alignas(config::kCacheLineSize) atomic_ref_wrapper
{
    alignas(std::atomic_ref<T>::required_alignment) T val;
};

/**
1. alignas(config::kCacheLineSize)  强制 atomic_ref_wrapper
结构体按缓存行大小（通常64字节）对齐，确保每个包装器独占一个缓存行
2. alignas(std::atomic_ref<T>::required_alignment) T val     成员 val 按 std::atomic_ref 要求的对齐方式对齐

*/

}; // namespace tmms::base