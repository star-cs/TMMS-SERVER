/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:57:38
 * @LastEditTime: 2025-07-26 22:20:33
 * @FilePath: /TMMS-SERVER/tmms/base/utils/utils.cpp
 * @Description:
 */
#include "utils.h"
#include "base/log/log.h"
#include <cxxabi.h>
#include <execinfo.h>
#include <filesystem>
#include <unistd.h>

namespace tmms::base
{

std::string Utils::get_bin_path()
{
    char    dir[4096] = {0};
    ssize_t n         = readlink("/proc/self/exe", dir, 4096);
    if (n <= 0)
    {
        static std::string empty_str;
        return empty_str;
    }

    std::filesystem::path path(std::string(dir, n));
    auto                  p = path.remove_filename();
    return p.string();
}

/// @brief 获取当前的精确时间（1970.1.1到现在的），单位是ms
/// @return 精确时间，单位：ms
int64_t TTime::NowMS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);                     // 获取精确时间到结构体
    return tv.tv_sec * 1000 + tv.tv_usec / 1000; // 一个是s,一个是us，换算成ms
}

/// @brief 获取当前的UTC时间
/// @return 返回数值，单位是s
int64_t TTime::Now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

/// @brief 获取年月日，时分秒
/// @param year
/// @param month
/// @param day
/// @param hour
/// @param minute
/// @param second
/// @return struct tm的时间结构体
int64_t TTime::Now(int& year, int& month, int& day, int& hour, int& minute, int& second)
{
    struct tm tm;
    time_t    t = time(NULL); // 获取本地时间
    localtime_r(&t, &tm);     // 本地时间转化到时间结构体中

    year   = tm.tm_year + 1900; // 内部-1900了，所以要加1900
    month  = tm.tm_mon + 1;     // 内部是[0-11]的数字，所以+1
    day    = tm.tm_mday;
    hour   = tm.tm_hour;
    minute = tm.tm_min;
    second = tm.tm_sec;

    return t;
}

/// @brief 过去当前时间IOS字符串
/// @return 字符串的时间格式
std::string TTime::ISOTime()
{
    struct timeval tv;
    struct tm      tm;

    gettimeofday(&tv, NULL);
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    char buf[128] = {0};
    auto n        = sprintf(
        buf,
        "%4d-%02d-%02dT%02d:%02d:%02d",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec);
    return std::string(buf, buf + n);
}
/**
 * @brief 解析C++符号的混淆名称（demangle）
 *
 * 该函数尝试解析两种格式的混淆符号：
 * 1. 包含在括号中的复杂符号（如类型信息）
 * 2. 简单的符号字符串
 *
 * @param str 需要解析的混淆符号字符串指针
 * @return std::string 解析后的可读字符串，若解析失败返回原始字符串或简单符号
 */
static std::string demangle(const char* str)
{
    size_t      size   = 0;
    int         status = 0;
    std::string rt;
    rt.resize(256);

    // 尝试解析带括号的复杂符号格式（如类型信息）
    // 格式说明：跳过'('前内容，跳过'_'前内容，捕获到')'或'+'前的内容
    if (1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0]))
    {
        // 使用ABI函数进行demangle处理
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if (v)
        {
            std::string result(v);
            free(v);
            return result;
        }
    }

    // 若复杂符号解析失败，尝试解析简单符号格式
    if (1 == sscanf(str, "%255s", &rt[0]))
    {
        return rt;
    }

    // 完全解析失败时返回原始字符串
    return str;
}

void Backtrace(std::vector<std::string>& bt, int size, int skip)
{
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s     = ::backtrace(array, size);

    char** strings = backtrace_symbols(array, s);
    if (strings == NULL)
    {
        CORE_ERROR("backtrace_synbols error");
        return;
    }

    for (size_t i = skip; i < s; ++i)
    {
        bt.push_back(demangle(strings[i]));
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix)
{
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for (size_t i = 0; i < bt.size(); ++i)
    {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

auto set_fd_noblock(int fd) noexcept -> void
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);

    flags |= O_NONBLOCK;
    assert(fcntl(fd, F_SETFL, flags) >= 0);
}

auto get_null_fd() noexcept -> int
{
    auto fd = open("/dev/null", O_RDWR);
    return fd;
}

} // namespace tmms::base