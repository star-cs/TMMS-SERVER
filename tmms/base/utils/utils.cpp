/*
 * @Author: star-cs
 * @Date: 2025-07-21 13:57:38
 * @LastEditTime: 2025-07-21 22:47:58
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

} // namespace tmms::base