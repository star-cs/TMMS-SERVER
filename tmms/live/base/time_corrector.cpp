#include "time_corrector.h"
#include "live/base/codec_utils.h"

namespace tmms::live
{
// 校正时间戳
uint32_t TimeCorrector::CorrectTimestamp(const PacketPtr& packet)
{
    // 完整的数据包，整个packet使用flv包装
    if (!CodecUtils::IsCodecHeader(packet))
    {
        int32_t pt = packet->PacketType();
        // 包的类型是视频包
        if (pt == kPacketTypeVideo)
        {
            return CorrectVideoTimeStampByVideo(packet);
        }
        else if (pt == kPacketTypeAudio)
        {
            return CorrectAudioTimeStampByAudio(packet);
        }
    }
    return 0;
}

// 没有连续的音频帧，以视频为基准校正（因为直播中视频比音频重要）
uint32_t TimeCorrector::CorrectAudioTimeStampByVideo(const PacketPtr& packet)
{
    ++audio_numbers_between_video_;
    if (audio_numbers_between_video_ > 1)
    {
        return CorrectAudioTimeStampByAudio(packet);
    }

    int64_t time = packet->TimeStamp();

    if (video_original_timestamp_ == -1)
    {
        audio_original_timestamp_  = time;
        audio_corrected_timestamp_ = time;
        return time;
    }

    // 使用视频的时间戳校正音频的
    int64_t delta = time - video_original_timestamp_;
    bool    fine  = (delta > -kMaxAudioDeltaTime) && (delta < kMaxAudioDeltaTime);
    if (!fine)
    {
        delta = kDefaultAudioDeltaTime;
    }

    audio_original_timestamp_  = time;
    audio_corrected_timestamp_ = video_corrected_timestamp_ + delta;
    if (audio_corrected_timestamp_ < 0)
    {
        audio_corrected_timestamp_ = 0;
    }

    return audio_corrected_timestamp_;
}

// 视频校正
uint32_t TimeCorrector::CorrectVideoTimeStampByVideo(const PacketPtr& packet)
{
    audio_numbers_between_video_ = 0; // 这是一个新的视频帧，两个视频帧之间的音频数初始化0
    int64_t time                 = packet->TimeStamp();
    if (video_original_timestamp_ == -1)
    {
        video_original_timestamp_  = time;
        video_corrected_timestamp_ = time;

        // 如果音频包比视频包来的快
        if (audio_original_timestamp_ != -1)
        {
            // 如果视频包和音频包相差的间隔很大
            int32_t delta = audio_original_timestamp_ - video_original_timestamp_;
            if (delta <= -kMaxVideoDeltaTime || delta >= kMaxVideoDeltaTime)
            {
                // 就第一个以音频时间戳为准
                video_original_timestamp_  = audio_original_timestamp_;
                video_corrected_timestamp_ = audio_corrected_timestamp_;
            }
        }
    }

    int64_t delta = time - video_original_timestamp_;
    bool    fine  = (delta > -kMaxVideoDeltaTime) && (delta < kMaxVideoDeltaTime);
    if (!fine)
    {
        delta = kDefaultVideoDeltaTime;
    }
    video_original_timestamp_ = time;
    video_corrected_timestamp_ += delta; // 上次的正确时间 + 时间差=这次正确的时间
    if (video_corrected_timestamp_ < 0)  // 时间戳<0，直接置零
    {
        video_corrected_timestamp_ = 0;
    }
    return video_corrected_timestamp_;
}

// 在两个视频帧之间有多个音频帧的时候，用音频自己的时间戳校正
uint32_t TimeCorrector::CorrectAudioTimeStampByAudio(const PacketPtr& packet)
{
    int64_t time = packet->TimeStamp();

    // 第一个音频包，直接返回，不用修正
    if (audio_original_timestamp_ == -1)
    {
        audio_original_timestamp_  = time;
        audio_corrected_timestamp_ = time;
        return time;
    }
    // 本次的时间 - 上一次的时间
    int64_t delta = time - audio_original_timestamp_;
    // 时间差不在一个范围内，就取默认的
    bool fine = (delta > -kMaxAudioDeltaTime) && (delta < kMaxAudioDeltaTime);
    if (!fine)
    {
        delta = kDefaultAudioDeltaTime;
    }

    audio_original_timestamp_ = time;
    audio_corrected_timestamp_ += delta; // 上次的正确时间 + 时间差=这次正确的时间
    if (audio_corrected_timestamp_ < 0)  // 时间戳<0，直接置零
    {
        audio_corrected_timestamp_ = 0;
    }
    return audio_corrected_timestamp_;
}

} // namespace tmms::live