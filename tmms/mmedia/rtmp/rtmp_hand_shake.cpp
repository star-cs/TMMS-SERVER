/*
 * @Author: star-cs
 * @Date: 2025-07-27 14:51:55
 * @LastEditTime: 2025-07-28 12:41:41
 * @FilePath: /TMMS-SERVER/tmms/mmedia/rtmp/rtmp_hand_shake.cpp
 * @Description:
 */
#include "mmedia/rtmp/rtmp_hand_shake.h"
#include "base/log/log.h"
#include "base/utils/utils.h"
#include <cstring>
#include <random>

// 匿名namespace 内部链接，替代static关键字的功能
namespace
{
/// @brief 服务端的版本号，4字节，协议规定的这些值
/// 0x0D:(CR)回车符， 0x0E:版本号 , 0x0A:(LR)换行符
static constexpr unsigned char rtmp_server_ver[4] = {0x0D, 0x0E, 0x0A, 0x0D};

/// @brief  客户端的版本号，4字节
/// 0x0C,表示换页符 , 0x00 :这是一个空字节，通常用作分隔符、填充或表示结束 , 0x0E : 版本号
static constexpr unsigned char rtmp_client_ver[4] = {0x0C, 0x00, 0x0D, 0x0E};

///  以固定的key,做hmac-sha256计算得到digest。下面两个是固定的key
#define PLAYER_KEY_OPEN_PART_LEN 30       ///< length of partial key used for first client digest signing
/** Client key used for digest signing */ // 都是协议固定的值
static constexpr uint8_t rtmp_player_key[] = {
    'G',  'e',  'n',  'u',  'i',  'n',  'e',  ' ',  'A',  'd',  'o',  'b',  'e',  ' ',  'F',  'l',
    'a',  's',  'h',  ' ',  'P',  'l',  'a',  'y',  'e',  'r',  ' ',  '0',  '0',  '1',

    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
    0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE};

#define SERVER_KEY_OPEN_PART_LEN 36 ///< length of partial key used for first server digest signing
/** Key used for RTMP server digest signing */
static constexpr uint8_t rtmp_server_key[] = {
    'G',  'e',  'n',  'u',  'i',  'n',  'e',  ' ',  'A',  'd',  'o',  'b',  'e',  ' ',  'F',  'l',  'a',  's',
    'h',  ' ',  'M',  'e',  'd',  'i',  'a',  ' ',  'S',  'e',  'r',  'v',  'e',  'r',  ' ',  '0',  '0',  '1',

    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57, 0x6E, 0xEC,
    0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE};

/// @brief 使用固定的key 计算digest ，其中digest-data的大小是固定32bytes
/// @param src  源buffer
/// @param len  buffer长度
/// @param gap  指定中间有没有间隔，间隔就是digest-data的值，没有就计算左右两边
/// @param key  固定的，客户端和服务端不同
/// @param keylen key的长度
/// @param dst 加密之后的结果
void CalculateDigest(const uint8_t* src, int len, int gap, const uint8_t* key, int keylen, uint8_t* dst)
{
    uint32_t digestLen = 0;
#if OPENSSL_VERSION_NUMBER > 0x10100000L
    HMAC_CTX* ctx;
#else
    HMAC_CTX ctx;
#endif
    HMAC_setup(ctx, key, keylen);
    if (gap <= 0)
    {
        HMAC_crunch(ctx, src, len);
    }
    else
    {
        HMAC_crunch(ctx, src, gap);
        // src+gap+SHA256_DIGEST_LENGTH  右边
        // len-gap-SHA256_DIGEST_LENGTH 左边
        HMAC_crunch(ctx, src + gap + SHA256_DIGEST_LENGTH, len - gap - SHA256_DIGEST_LENGTH);
    }
    HMAC_finish(ctx, dst, digestLen);
}

/// @brief sha算法不可逆，要再计算携带的消息的digest和自己保存是是否相同
/// @param buf  接收的消息
/// @param digest_pos diest-data所在的位置
/// @param key
/// @param keyLen
/// @return 对比digest-data是否相同
bool VerifyDigest(uint8_t* buf, int digest_pos, const uint8_t* key, size_t keyLen)
{
    uint8_t digest[SHA256_DIGEST_LENGTH];
    // 计算新的加密值
    CalculateDigest(buf, 1536, digest_pos, key, keyLen, digest);
    // 对比digest-data是否相同
    return memcmp(&buf[digest_pos], digest, SHA256_DIGEST_LENGTH) == 0;
}

/// @brief 获取在c1 / s1结构中的digest的偏移位置
/// @param buf  c1 / s1的数据
/// @param off 整个digest的总偏移值
/// @param mod_val 余数（其实是固定的 764 - 4 - 32）
/// @return digest-data的总偏移量
int32_t GetDigestOffset(const uint8_t* buf, int off, int mod_val)
{
    uint32_t offset = 0;
    // off有两种，一个是4字节，一个是4+764字节，看怎么排放的
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buf + off);
    uint32_t       res;

    // digest的开头是data的offset，4字节，现在加起来
    offset = ptr[0] + ptr[1] + ptr[2] + ptr[3];
    // offset % mod_val ： 怕超出764字节
    // off + 4： 总偏移量 + 前面的timestamp + version(4bytes)
    res = (offset % mod_val) + (off + 4);
    return res;
}
} // namespace

namespace tmms::mm
{
RtmpHandShake::RtmpHandShake(const TcpConnection::ptr& conn, bool client) : connection_(conn), is_client_(client)
{
}

void RtmpHandShake::Start()
{
    CreateC1S1(); // 携带C0C1
    if (is_client_)
    { // 客户端就发送C0C1
        state_ = kHandShakePostC0C1;
        SendC1S1();
    }
    else
    { // 服务端先等待
        state_ = kHandShakeWaitC0C1;
    }
}

/// @brief 服务端和客户端的接收流程
/// @param buf
/// @return 1表示数据不够，接着发送；-1表示检测失败；2表示中间状态；0成功
int32_t RtmpHandShake::HandShake(MsgBuffer& buf)
{
    switch (state_)
    {
        case kHandShakeWaitC0C1: // 服务端等待客户端发送C0C1，收到就是进行检测，处理，然后发送S0S1
        {
            if (buf.ReadableBytes() < kRtmpHandShakePacketSize + 1) // 数据不够C0C1 / S0S1
            {
                return 1;
            }
            RTMP_TRACE("host:{} Recv C0C1", connection_->PeerAddr().ToIpPort());

            auto offset = CheckC1S1(buf.Peek(), kRtmpHandShakePacketSize + 1);
            if (offset >= 0)
            {
                // C1接收成功了，创建S2准备下一轮的发送
                CreateC2S2(buf.Peek() + 1, kRtmpHandShakePacketSize, offset);
                buf.Retrieve(kRtmpHandShakePacketSize + 1);
                state_ = kHandShakePostS0S1;
                SendC1S1(); // S0S1
            }
            else
            {
                RTMP_TRACE("host:{} Check C0C1 error", connection_->PeerAddr().ToIpPort());
                return -1;
            }
            break;
        }
        case kHandShakeWaitC2: // 服务端收到客户端发送C2的状态，就可以检测，标记结束
        {
            if (buf.ReadableBytes() < kRtmpHandShakePacketSize)
            {
                return 1;
            }
            RTMP_TRACE("host:{} Recv C2", connection_->PeerAddr().ToIpPort());
            if (CheckC2S2(buf.Peek(), kRtmpHandShakePacketSize))
            {
                buf.Retrieve(kRtmpHandShakePacketSize);
                RTMP_TRACE("host:{} handshake done", connection_->PeerAddr().ToIpPort());
                state_ = kHandShakeDone;
                return 0;
            }
            else
            {
                RTMP_TRACE("host:{} Check C2 error", connection_->PeerAddr().ToIpPort());
                return -1;
            }
            break;
        }
        case kHandShakeWaitS0S1: // 客户端等到服务端的S0S1
        {
            if (buf.ReadableBytes() < kRtmpHandShakePacketSize)
            {
                return 1;
            }
            RTMP_TRACE("host:{} Recv S0S1", connection_->PeerAddr().ToIpPort());
            auto offset = CheckC1S1(buf.Peek(), kRtmpHandShakePacketSize + 1);
            if (offset >= 0)
            {
                // S0S1 接受完，创建C2准备，接收到S2时发送。
                CreateC2S2(buf.Peek() + 1, kRtmpHandShakePacketSize, offset);
                buf.Retrieve(kRtmpHandShakePacketSize + 1);
                // 因为服务端发送完了S0S1，可以接着就发S2了，判断有没有收到S2
                if (buf.ReadableBytes() == kRtmpHandShakePacketSize) // S2
                {
                    RTMP_TRACE("host:{}, Recv S2.", connection_->PeerAddr().ToIpPort());
                    state_ = kHandShakeDoning; // 等待C2发送完
                    buf.Retrieve(kRtmpHandShakePacketSize);
                    SendC2S2();
                    return 0;
                }
                else
                {
                    state_ = kHandShakePostC2; // 状态变了，因为没有收到S2，就是准备发送C2的状态
                    SendC2S2();
                }
            }
            else
            {
                RTMP_TRACE("host:{} Check S0S1 error", connection_->PeerAddr().ToIpPort());
                return -1;
            }
            break;
        }
    }
    return 1;
}

/// @brief 客户端和服务端的发送完成状态
void RtmpHandShake::WriteComplete()
{
    switch (state_)
    {
        case kHandShakePostS0S1: // 服务端发送完S0S1，服务端可以直接发送S2
        {
            RTMP_TRACE("host:{} Post S0S1.", connection_->PeerAddr().ToIpPort());
            state_ = kHandShakePostS2;
            SendC2S2();
            break;
        }
        case kHandShakePostS2:
        {
            RTMP_TRACE("host:{} Post S2.", connection_->PeerAddr().ToIpPort());
            state_ = kHandShakeWaitC2; // 等客户端的C2
            break;
        }
        case kHandShakePostC0C1:
        {
            RTMP_TRACE("host:{} Post C0C1.", connection_->PeerAddr().ToIpPort());
            state_ = kHandShakeWaitS0S1;
            break;
        }
        case kHandShakePostC2: // 客户端收到S0S1之后，但是没有连续收到S2，就变为PostC2完成状态 。客户端PostC2就完成
        {
            RTMP_TRACE("host:{} Post C0C1.", connection_->PeerAddr().ToIpPort());
            state_ = kHandShakeDone;
            break;
        }
        case kHandShakeDoning:
        {
            RTMP_TRACE("host:{} Post S2.done .", connection_->PeerAddr().ToIpPort());
            state_ = kHandShakeDone;
            break;
        }
    }
}

/// @brief 使用c++11的mt19937随机数产生
/// @return 一个随机数，填充digest
uint8_t RtmpHandShake::GenRandom()
{
    // 初始化随机数生成器
    std::random_device rd;
    std::mt19937       mt(rd());
    // 离散均匀分布类，指定随机数范围,不超过uint8_t, 1字节
    std::uniform_int_distribution<> rand(0, 256);
    return rand(mt) % 256;
}

/// @brief C0C1，S0S1一起生成，所以长度为 kRtmpHandShakePacketSize + 1
void RtmpHandShake::CreateC1S1()
{
    // C1S1，除了数据都是随机数
    // 所以先全部填充随机数，再填充数据
    for (int i = 0; i < kRtmpHandShakePacketSize + 1; i++)
    {
        C1S1_[i] = GenRandom();
    }

    C1S1_[0] = '\x03';          // 版本号1字节，版本号为3，C1S1_[0]表示C0S0_
    memset(C1S1_ + 1, 0x00, 4); // time 时间戳，取值为0或者其他任意值

    if (!is_complex_handshake_)
    {
        // 简单握手
        // 4个字节的zero
        memset(C1S1_ + 5, 0x00, 4);
        // 1528字节的任意数据 由于握手的双方需要区分另一端，此字段填充的数据必须足够随机以防止与其他握手端混淆
    }
    else
    {
        // 复杂握手
        // 使用 digest 在 key前面的格式

        // 复杂握手中，key是随机生成的128字节数据
        auto     offset = GetDigestOffset(C1S1_ + 1, 8, 728);
        uint8_t* data   = C1S1_ + 1 + offset;

        // 4个字节的version。
        if (is_client_)
        {
            memcpy(C1S1_ + 5, rtmp_client_ver, 4);

            // 计算 digest 密文
            CalculateDigest(
                C1S1_ + 1, kRtmpHandShakePacketSize, offset, rtmp_player_key, PLAYER_KEY_OPEN_PART_LEN, data);
        }
        else
        {
            memcpy(C1S1_ + 5, rtmp_server_ver, 4);
            CalculateDigest(
                C1S1_ + 1, kRtmpHandShakePacketSize, offset, rtmp_server_key, SERVER_KEY_OPEN_PART_LEN, data);
        }
        memcpy(digest_, data, SHA256_DIGEST_LENGTH); // 复制 digest
    }
}
/// @brief 校验C1S1正确性
/// @param data
/// @param bytes
/// @return >0，digest的offset的值；-1失败；==0表示简单模式
int32_t RtmpHandShake::CheckC1S1(const char* data, int bytes)
{
    if (bytes != 1537)
    {
        RTMP_ERROR("unexpected c1s1, len = {}", bytes);
    }
    if (data[0] != '\x03') // 版本号错误,data包含s0c0
    {
        RTMP_ERROR("unexpected c1s1, version={}", data[0]);
        return -1;
    }

    uint32_t* version = (uint32_t*)(data + 5); // 简单握手的第五个字节是0
    if (*version == 0)
    {
        is_complex_handshake_ = false;
        return 0;
    }

    int32_t offset = 0;
    if (is_complex_handshake_) // 复杂握手只要获取digest-data部分进行加密验证就行，其他都是随机数
    {
        uint8_t* handshake = (uint8_t*)(data + 1);
        offset             = GetDigestOffset(handshake, 8, 728); // 获取digest-data
        if (is_client_)
        {
            // 验证第一种格式，digest在key前面的
            // 这里客户端验证的是服务端的key
            if (!VerifyDigest(handshake, offset, rtmp_server_key, SERVER_KEY_OPEN_PART_LEN))
            {
                // 第一种不是，验证第二种的，diegst在key后面的
                offset = GetDigestOffset(handshake, 772, 728);
                if (!VerifyDigest(handshake, offset, rtmp_server_key, SERVER_KEY_OPEN_PART_LEN))
                {
                    return -1; // 都验证失败，错误
                }
            }
        }
        else
        {
            // 验证第一种格式，digest在key前面的
            // 这里客户端验证的是服务端的key
            if (!VerifyDigest(handshake, offset, rtmp_player_key, PLAYER_KEY_OPEN_PART_LEN))
            {
                // 第一种不是，验证第二种的，diegst在key后面的
                offset = GetDigestOffset(handshake, 772, 728);
                if (!VerifyDigest(handshake, offset, rtmp_player_key, PLAYER_KEY_OPEN_PART_LEN))
                {
                    return -1; // 都验证失败，错误
                }
            }
        }
    }
    return offset;
}

void RtmpHandShake::SendC1S1()
{
    connection_->Send((const char*)C1S1_, 1537); // C0C1或者S0S1都是一起发送的，C0一个字节，C1 1536字节
}

/// @brief digest-data计算方法：
// S2：先通过C1的digest，计算出key，再用这个key计算random-data的digest。
// C2：先通过S1的digest，计算出key，再用这个key计算random-data的digest。
/// @param data  data是C1S1的前八字节,timestamp和version
/// @param bytes
/// @param offset
void RtmpHandShake::CreateC2S2(const char* data, int bytes, int offset)
{
    for (int i = 0; i < kRtmpHandShakePacketSize; i++)
    {
        C2S2_[i] = GenRandom();
    }
    if (!is_complex_handshake_)
    {
        // 简单模式
        // 4字节 time 发送时刻的时间戳
        // 4字节 time2 接受对端发送过来的握手包时刻
        // 1528字节 随机数据
        memcpy(C2S2_, data, 8);              // data是C1S1的前八字节,timestamp和version
        auto timestamp = base::TTime::Now(); // 设置当前时间戳，version不变
        // 设置当前时间戳，version不变
        char* t = (char*)&timestamp; // C2S2_是单字节的数组，要转换下

        // 网络端要转换字节序，小端变大端 ，timestamp一共4字节
        C2S2_[3] = t[0];
        C2S2_[2] = t[1];
        C2S2_[1] = t[2];
        C2S2_[0] = t[3];
    }
    else
    {
        // 复杂模式
        // 1504 字节 随机数据
        // 32 字节  random-data 的 digest-data

        // S2：先通过C1的digest，计算出key，再用这个key计算random-data的digest。
        // C2：先通过S1的digest，计算出key，再用这个key计算random-data的digest。
        uint8_t digest[32]; // key
        if (is_client_)
        {
            // 计算C1S1的digest_，客户端就计算自己的，填充C2
            CalculateDigest(digest_, 32, 0, rtmp_player_key, sizeof(rtmp_player_key), digest);
        }
        else
        {
            CalculateDigest(digest_, 32, 0, rtmp_server_key, sizeof(rtmp_server_key), digest);
        }
        // 计算真正的digest 32 bytes
        CalculateDigest(C2S2_, kRtmpHandShakePacketSize - 32, 0, digest, 32, &C2S2_[kRtmpHandShakePacketSize - 32]);
    }
}

/// @brief 一般可以不检测，S1S2之后就可以发数据了
/// @param data
/// @param bytes
/// @return
bool RtmpHandShake::CheckC2S2(const char* data, int bytes)
{
    // 暂时不检测
    return true;
}

void RtmpHandShake::SendC2S2()
{
    connection_->Send((const char*)C2S2_, kRtmpHandShakePacketSize);
}

} // namespace tmms::mm