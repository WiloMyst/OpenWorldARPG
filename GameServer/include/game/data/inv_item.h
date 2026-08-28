#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace game {
namespace data {

// 背包物品的内存表示, 与 proto ItemInstance 一一对应
// 仅动态数据: 静态规则 (堆叠/分类) 经 item_id 查 ItemConfig
struct InvItem {
    std::string guid;
    int32_t item_id = 0;
    int32_t count = 0;
    int64_t acquired_time = 0;
};

} // namespace data
} // namespace game
