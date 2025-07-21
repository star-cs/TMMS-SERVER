#pragma once

#include "noncopyable.h"
#include <atomic>
#include <mutex>
#include <semaphore.h>
#include <shared_mutex>
#include <stdint.h>

#if defined(_MSC_VER)
    #include <immintrin.h>
#endif

namespace tmms::base
{

class Semaphore : public NonCopyable
{
public:
    Semaphore(uint32_t count = 0)
    {
        if (sem_init(&m_semaphore, 0, count))
        {
            throw std::logic_error("sem_init error");
        }
    }

    ~Semaphore() { sem_destroy(&m_semaphore); }

    void wait()
    {
        if (sem_wait(&m_semaphore))
        {
            throw std::logic_error("sem_wait error");
        }
    }

    void notify()
    {
        if (sem_post(&m_semaphore))
        {
            throw std::logic_error("sem_post error");
        }
    }

private:
    sem_t m_semaphore;
};

// 读写锁
class RWMutex : public NonCopyable
{
public:
    // 使用标准库的RAII锁类型
    class ReadLock
    {
    public:
        explicit ReadLock(RWMutex& mtx) : m_lock(mtx.m_mutex) {}

    private:
        std::shared_lock<std::shared_mutex> m_lock;
    };

    class WriteLock
    {
    public:
        explicit WriteLock(RWMutex& mtx) : m_lock(mtx.m_mutex) {}

    private:
        std::unique_lock<std::shared_mutex> m_lock;
    };

    void rdlock() { m_mutex.lock_shared(); }
    void wrlock() { m_mutex.lock(); }

private:
    std::shared_mutex m_mutex; // C++17标准读写锁
};

// 互斥锁
class Mutex : public NonCopyable
{
public:
    using Lock = std::unique_lock<std::mutex>;

    Mutex()  = default;
    ~Mutex() = default;

    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }

    // 提供标准库互斥量的访问（用于条件变量等场景）
    std::mutex& native_handle() { return m_mutex; }

private:
    std::mutex m_mutex;
};

// 局部锁的模板实现  RAII
template<class T>
struct ScopedLockImpl : public NonCopyable
{
public:
    ScopedLockImpl(T& mutex) : m_mutex(mutex)
    {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl() { unlock(); }

    void lock()
    {
        if (!m_locked)
        {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    // mutex
    T&   m_mutex;
    bool m_locked;
};

// 自旋锁
using std::atomic;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
struct SpinkLock : public NonCopyable
{
    using Lock = ScopedLockImpl<SpinkLock>;

    atomic<bool> lock_ = {0};

    void lock() noexcept
    {
        for (;;)
        {
            // Optimistically assume the lock is free on the first try
            if (!lock_.exchange(true, memory_order_acquire))
            {
                return;
            }
            // Wait for lock to be released without generating cache misses
            while (lock_.load(memory_order_relaxed))
            {
                // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
                // hyper-threads

#if defined(__GNUC__) || defined(__clang__)
                __builtin_ia32_pause();
#elif defined(_MSC_VER)
                _mm_pause();
#else
                __builtin_ia32_pause();
#endif
            }
        }
    }

    bool try_lock() noexcept
    {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !lock_.load(memory_order_relaxed) && !lock_.exchange(true, memory_order_acquire);
    }

    void unlock() noexcept { lock_.store(false, memory_order_release); }
};

} // namespace mms::base