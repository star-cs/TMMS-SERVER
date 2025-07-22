#pragma once
#include <string>
#include <vector>

namespace tmms::base
{
class Utils
{
public:
    static std::string get_bin_path();
};

class TTime
{
public:
    static int64_t     NowMS();
    static int64_t     Now();
    static int64_t     Now(int& year, int& month, int& day, int& hour, int& minute, int& second);
    static std::string ISOTime();
};

/**
 * @brief 获取当前的调用栈
 * @param[out] bt 保存调用栈
 * @param[in] size 最多返回层数
 * @param[in] skip 跳过栈顶的层数
 */
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

} // namespace tmms::base
