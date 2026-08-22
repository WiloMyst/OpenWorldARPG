#pragma once
#include <chrono>
#include <cstdint>

namespace game {
namespace infra {

inline int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace infra
} // namespace game
