/*
 * @Author: star-cs
 * @Date: 2025-07-27 09:48:42
 * @LastEditTime: 2025-07-27 10:22:55
 * @FilePath: /TMMS-SERVER/tmms/base/utils/stringutils.h
 * @Description:
 */
#pragma once

#include <string>
#include <vector>
namespace tmms::base
{
using std::string;
using std::vector;

class StringUtils // 工具类，所有函数成员定义成静态的
{
public:
    /// 1. 字符串匹配工具
    // 前缀匹配
    static bool StartsWith(const string& s, const string& sub);
    // 后缀匹配
    static bool EndWith(const string& s, const string& sub);

public:
    /// 2. 文件名，文件路径操作
    // 从完整的文件路径，文件所在的父目录
    static string FilePath(const string& path);
    // 完整的文件路径中，取出文件名+文件后缀
    static string FileNameExt(const string& path);
    // 完整的文件路径中，返回文件名
    static string FileName(const string& path);
    // 完整的文文件路径中，返回文件后缀
    static string Extension(const string& path);

public:
    /// 3. 字符串分割
    /// 把一个字符串按照分隔符分割成vector
    static vector<string> SplitString(const string& s, const string& delimiter);

    // 有限状态机
    static vector<string> SplitStringWithFSM(const string& s, const char delimiter);
};
} // namespace tmms::base