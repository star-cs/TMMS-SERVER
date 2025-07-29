#pragma once

#include "network/net/tcpconnection.h"

#include <memory>
#include <openssl/hmac.h>
#include <openssl/sha.h>

/// 借用openssl的hmac-sha算法加密
/// 因为版本不同,接口不同，选用可以使用的版本
#if OPENSSL_VERSION_NUMBER > 0x10100000L
    #define HMAC_setup(ctx, key, len)                                                                                  \
        ctx = HMAC_CTX_new();                                                                                          \
        HMAC_Init_ex(ctx, key, len, EVP_sha256(), 0)
    #define HMAC_crunch(ctx, buf, len) HMAC_Update(ctx, buf, len)
    #define HMAC_finish(ctx, dig, dlen)                                                                                \
        HMAC_Final(ctx, dig, &dlen);                                                                                   \
        HMAC_CTX_free(ctx)
#else
    #define HMAC_setup(ctx, key, len)                                                                                  \
        HMAC_CTX_init(&ctx);                                                                                           \
        HMAC_Init_ex(&ctx, key, len, EVP_sha256(), 0)
    #define HMAC_crunch(ctx, buf, len) HMAC_Update(&ctx, buf, len)
    #define HMAC_finish(ctx, dig, dlen)                                                                                \
        HMAC_Final(&ctx, dig, &dlen);                                                                                  \
        HMAC_CTX_cleanup(&ctx)
#endif

namespace tmms::mm
{
using namespace tmms::net;

constexpr int kRtmpHandShakePacketSize = 1536;

enum RtmpHandShakeState
{
    kHandShakeInit,
    // client
    kHandShakePostC0C1, // 发送C0C1
    kHandShakeWaitS0S1, // 等待S0S1
    kHandShakePostC2,
    kHandShakeWaitS2,
    kHandShakeDoning, // 结束中，因为发送完S1可以直接发送S2，客户端可能就处于等待接受处理中
    // server
    kHandShakeWaitC0C1,
    kHandShakePostS0S1,
    kHandShakePostS2,
    kHandShakeWaitC2,
    kHandShakeDone,
};

class RtmpHandShake
{
public:
    using ptr = std::shared_ptr<RtmpHandShake>;
    
    // 因为要区分服务端和客户端
    RtmpHandShake(const TcpConnectionPtr& conn, bool client);
    ~RtmpHandShake() = default;
    void Start();

    int32_t HandShake(MsgBuffer& buf);
    void    WriteComplete();

private:
    uint8_t GenRandom();

    void    CreateC1S1();
    int32_t CheckC1S1(const char* data, int bytes);
    void    SendC1S1();

    void CreateC2S2(const char* data, int bytes, int offset);
    bool CheckC2S2(const char* data, int bytes);
    void SendC2S2();

    TcpConnectionPtr connection_;
    bool             is_client_{false};                   // 默认服务端
    bool             is_complex_handshake_{true};         // 默认复杂握手，digest失败或者明确指定才使用简单的握手
    uint8_t          digest_[SHA256_DIGEST_LENGTH];       // digest-data
    uint8_t          C1S1_[kRtmpHandShakePacketSize + 1]; // c1 s1公用，不是同一端的1537字节固定
    uint8_t          C2S2_[kRtmpHandShakePacketSize];

    int32_t state_{kHandShakeInit};
};

} // namespace tmms::mm