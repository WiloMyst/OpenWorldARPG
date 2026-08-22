#pragma once

// 自包含 HMAC-SHA256 (FIPS 198-1 / RFC 2104), 无 OpenSSL 依赖
// 用途: GameServer 签发对话票据 / VHServer 验证票据, 两端算法契约必须逐字节一致

#include <cstdint>
#include <cstring>
#include <string>

namespace crypto {

namespace detail {

inline constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t Rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buffer[64];
    size_t buflen;
};

inline void Sha256Init(Sha256Ctx& ctx) {
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.bitlen = 0;
    ctx.buflen = 0;
}

inline void Sha256Transform(Sha256Ctx& ctx, const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
    uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + S1 + ch + K[i] + w[i];
        const uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
}

inline void Sha256Update(Sha256Ctx& ctx, const uint8_t* data, size_t len) {
    ctx.bitlen += static_cast<uint64_t>(len) * 8;
    while (len > 0) {
        const size_t take = 64 - ctx.buflen < len ? 64 - ctx.buflen : len;
        std::memcpy(ctx.buffer + ctx.buflen, data, take);
        ctx.buflen += take;
        data += take;
        len -= take;
        if (ctx.buflen == 64) {
            Sha256Transform(ctx, ctx.buffer);
            ctx.buflen = 0;
        }
    }
}

inline void Sha256Final(Sha256Ctx& ctx, uint8_t out[32]) {
    ctx.buffer[ctx.buflen++] = 0x80;
    if (ctx.buflen > 56) {
        std::memset(ctx.buffer + ctx.buflen, 0, 64 - ctx.buflen);
        Sha256Transform(ctx, ctx.buffer);
        ctx.buflen = 0;
    }
    std::memset(ctx.buffer + ctx.buflen, 0, 56 - ctx.buflen);
    const uint64_t bits = ctx.bitlen;
    for (int i = 0; i < 8; ++i) {
        ctx.buffer[56 + i] = static_cast<uint8_t>(bits >> (56 - i * 8));
    }
    Sha256Transform(ctx, ctx.buffer);
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>(ctx.state[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(ctx.state[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(ctx.state[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i]);
    }
}

} // namespace detail

// HMAC-SHA256, 输出 32 字节摘要
inline void HmacSha256(const std::string& key, const std::string& message, uint8_t out[32]) {
    uint8_t k[64];
    std::memset(k, 0, sizeof(k));
    if (key.size() > 64) {
        detail::Sha256Ctx h;
        detail::Sha256Init(h);
        detail::Sha256Update(h, reinterpret_cast<const uint8_t*>(key.data()), key.size());
        detail::Sha256Final(h, k);
    } else {
        std::memcpy(k, key.data(), key.size());
    }

    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    detail::Sha256Ctx inner;
    detail::Sha256Init(inner);
    detail::Sha256Update(inner, ipad, 64);
    detail::Sha256Update(inner, reinterpret_cast<const uint8_t*>(message.data()), message.size());

    uint8_t inner_digest[32];
    detail::Sha256Final(inner, inner_digest);

    detail::Sha256Ctx outer;
    detail::Sha256Init(outer);
    detail::Sha256Update(outer, opad, 64);
    detail::Sha256Update(outer, inner_digest, 32);
    detail::Sha256Final(outer, out);
}

// HMAC-SHA256, 输出 64 字符小写十六进制串
inline std::string HmacSha256Hex(const std::string& key, const std::string& message) {
    uint8_t digest[32];
    HmacSha256(key, message, digest);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 32; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 0xf]);
    }
    return out;
}

// 常数时间比较, 防止按字节试错伪造签名
inline bool ConstTimeEqual(const std::string& a, const std::string& b) {
    const size_t n = a.size() < b.size() ? b.size() : a.size();
    uint8_t diff = static_cast<uint8_t>(a.size() == b.size() ? 0 : 1);
    for (size_t i = 0; i < n; ++i) {
        diff |= static_cast<uint8_t>(a[i < a.size() ? i : 0]) ^ static_cast<uint8_t>(b[i < b.size() ? i : 0]);
    }
    return diff == 0;
}

} // namespace crypto
