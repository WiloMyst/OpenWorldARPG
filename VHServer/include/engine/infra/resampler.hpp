#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace engine {
namespace infra {

/// @brief 轻量级 PCM 重采样工具（线性插值）
///
/// 用于解决 Piper TTS 输出采样率（22050Hz）与 NVIDIA Audio2Face
/// 推荐输入采样率（16kHz）不匹配的问题。当 src_rate == dst_rate 时
/// 直接拷贝返回，零开销。
class LinearResampler {
public:
    /// @brief 对 16-bit mono PCM 数据做线性插值重采样
    /// @param input 输入 PCM 样本
    /// @param src_rate 输入采样率（Hz）
    /// @param dst_rate 输出采样率（Hz）
    /// @return 重采样后的 PCM 样本
    static std::vector<int16_t> Resample(const std::vector<int16_t>& input,
                                         int src_rate,
                                         int dst_rate) {
        if (src_rate <= 0 || dst_rate <= 0) {
            throw std::invalid_argument("Sample rate must be positive");
        }

        // 采样率一致：直接拷贝，避免无谓计算
        if (src_rate == dst_rate) {
            return input;
        }

        if (input.empty()) {
            return {};
        }

        // 输出样本数 = 输入样本数 * (dst / src)
        // 向上取整保留末尾 fractional 帧的信息
        const double ratio = static_cast<double>(dst_rate) / static_cast<double>(src_rate);
        const size_t out_len = static_cast<size_t>(input.size() * ratio + 0.5);

        std::vector<int16_t> output;
        output.reserve(out_len);

        for (size_t i = 0; i < out_len; ++i) {
            // 输出样本 i 对应输入域的位置
            const double src_pos = static_cast<double>(i) / ratio;
            const size_t idx0 = static_cast<size_t>(src_pos);
            const double frac = src_pos - static_cast<double>(idx0);

            // 边界处理：最后一个样本无下一帧可插值，直接取末值
            if (idx0 + 1 >= input.size()) {
                output.push_back(input.back());
                continue;
            }

            // 线性插值: y = y0 + (y1 - y0) * frac
            const double y0 = static_cast<double>(input[idx0]);
            const double y1 = static_cast<double>(input[idx0 + 1]);
            const double interpolated = y0 + (y1 - y0) * frac;

            // 饱和截断到 int16 范围
            double clamped = interpolated < -32768.0 ? -32768.0 :
                             (interpolated > 32767.0 ? 32767.0 : interpolated);
            output.push_back(static_cast<int16_t>(clamped));
        }

        return output;
    }
};

} // namespace infra
} // namespace engine
