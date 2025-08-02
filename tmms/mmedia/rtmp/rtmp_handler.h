#pragma once

#include "mmedia/base/mmediahander.h"
namespace tmms::mm
{
class RtmpHandler : public MMediaHandler
{
public:
    // 播放
    virtual bool OnPlay(const TcpConnectionPtr& conn, const std::string& session_name, const std::string& param)
    {
        return false;
    }
    // 推流
    virtual bool OnPublish(const TcpConnectionPtr& conn, const std::string& session_name, const std::string& param)
    {
        return false;
    }

    // 暂停
    virtual bool OnPause(const TcpConnectionPtr& conn, bool pause) { return false; }

    // 定位(快进等)≈
    virtual void OnSeek(const TcpConnectionPtr& conn, double time) {}

    // 告知可以开始publish
    virtual void OnPublishPrepare(const TcpConnectionPtr& conn) {}
};
} // namespace tmms::mm