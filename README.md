# 流媒体服务器

## 1. tmms-base
- spdlog
- yamlcpp

## 2. 网络库
- event

- eventloop

- eventloopthread

- eventloopthreadpool

## 3. rtmp


## 4. live

### 时间戳校正
其中的 边界条件：
- MaxDelta：以帧率10计算，1/10 = 100ms
- VideoDefaultDelta: 以帧率25计算，1/25 = 40ms
- AudioDefaultDelta: 
    - AAC每帧数据对应字节数1024，假如sample_rate为44100，对应的事件剑客为 1024 * 1000/44100 = 23ms;
    - mp3每帧数据对应字节数1152，假如sample_rate为44100，对应事件间隔为 1152 * 1000/44100 = 26ms;
    - 最终取整数 20ms

视频时间戳较正：  
以视频自身时间戳为基准，通过前后帧的时间差维护连续的校正后时间戳。
- 重置 “两视频帧间的音频计数”（audio_numbers_between_video_），表示新视频帧出现。
- 首次处理视频时：初始化原始时间戳和校正后时间戳；若此前已有音频包，会检查音视频时间差是否过大（超过 kMaxVideoDeltaTime），若过大则以音频时间戳为基准初始化视频时间戳。
- 非首次处理：计算当前视频与上一视频的原始时间差，若差值在合理范围（-kMaxVideoDeltaTime 到 kMaxVideoDeltaTime）则直接使用，否则用默认差值（kDefaultVideoDeltaTime）；校正后时间戳 = 上一次校正值 + 差值（确保非负）。

音频时间戳校正：
- 以视频为基准校正（CorrectAudioTimeStampByVideo）
    - 两视频帧之间的音频包数量较少（≤1），优先跟随视频时间戳。
    - 若音频包数量超过 1，则切换到音频自校正（CorrectAudioTimeStampByAudio）。
    - 首次处理时：初始化音频原始时间戳和校正后时间戳。
    - 非首次处理：计算音频与当前视频原始时间的差值，若在合理范围（-kMaxAudioDeltaTime 到 kMaxAudioDeltaTime）则使用，否则用默认差值（kDefaultAudioDeltaTime）；校正后音频时间戳 = 视频校正后时间戳 + 差值（确保非负）。
- 音频自校正（CorrectAudioTimeStampByAudio）
    - 两视频帧之间的音频包数量较多（>1），通过音频自身前后帧维护连续性。
    - 首次处理时：初始化音频原始时间戳和校正后时间戳。
    - 非首次处理：计算当前与上一音频的原始时间差，若在合理范围则使用，否则用默认差值；校正后时间戳 = 上一次校正值 + 差值（确保非负）。

### GopMgr