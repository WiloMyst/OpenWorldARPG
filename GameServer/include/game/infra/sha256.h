#pragma once
// 自包含 SHA-256 (FIPS 180-4) 与常数时间字符串比较.
// 服务器现有依赖 (spdlog/yaml-cpp/nlohmann/mysql/hiredis/grpc) 无暴露可信哈希 API,
// 为认证加一套 OpenSSL 链接过于重, 故内联一份可验证的参考实现.
// 输入为 std::string 字节序列, 输出 32 字节摘要; hex 编码直接作为存储/比较载体.

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

namespace game {
namespace infra {

namespace detail {
inline uint32_t Rotr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}
}  // namespace detail

// 返回 SHA-256(data) 的 32 字节原始摘要
inline std::string Sha256(const std::string& data) {
    // 初始哈希值 (前 8 个质数平方根小数部分取高 32 位)
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    // 每轮常量 (前 64 个质数立方根小数部分取高 32 位)
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    // 填充: 原生 64-bit 计数, 在请求线程内单次计算 (哈希发生于低并发登录路径)
    const uint64_t bmlen = static_cast<uint64_t>(data.size()) * 8;
    std::string msg = data;
    msg.push_back(static_cast<char>(0x80));
    while ((msg.size() % 64) != 56) msg.push_back(static_cast<char>(0));
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<char>((bmlen >> (i * 8)) & 0xff));

    uint32_t w[64];
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        for (int i = 0; i < 16; ++i) {
            const size_t p = chunk + i * 4;
            w[i] = (static_cast<uint32_t>(static_cast<unsigned char>(msg[p])) << 24) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(msg[p + 1])) << 16) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(msg[p + 2])) << 8) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(msg[p + 3])));
        }
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = detail::Rotr(w[i - 15], 7) ^ detail::Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = detail::Rotr(w[i - 2], 17) ^ detail::Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t s1 = detail::Rotr(e, 6) ^ detail::Rotr(e, 11) ^ detail::Rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + s1 + ch + k[i] + w[i];
            const uint32_t s0 = detail::Rotr(a, 2) ^ detail::Rotr(a, 13) ^ detail::Rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = s0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::string out(32, '\0');
    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = static_cast<char>((h[i] >> 24) & 0xff);
        out[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xff);
        out[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xff);
        out[i * 4 + 3] = static_cast<char>(h[i] & 0xff);
    }
    return out;
}

// hex 编码 (二进制摘要 -> 存储/比较字符串)
inline std::string HexEncode(const std::string& bytes) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0x0f]);
    }
    return out;
}

// 生成随机盐 (16 字节, 每账号独立; 反查表由盐消解)
// std::random_device 映射到系统 CSPRNG (Linux: /dev/urandom; Windows: RtlGenRandom),
// 登录路径低并发, 初始化成本可接受
inline std::string RandomSalt() {
    std::array<uint8_t, 16> buf{};
    std::random_device rd;
    for (auto& b : buf) b = static_cast<uint8_t>(rd());
    return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
}

// 密码哈希: SHA256(salt + password), 返回 hex
inline std::string HashPassword(const std::string& salt, const std::string& password) {
    return HexEncode(Sha256(salt + password));
}

// 常数时间比较: 长度与内容均在固定时间内完成, 防时序侧信道 (消息循环固定步长)
inline bool ConstantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

}  // namespace infra
}  // namespace game