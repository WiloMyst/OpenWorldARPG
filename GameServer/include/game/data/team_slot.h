#pragma once
#include <cstdint>
#include <string>

namespace game {
namespace data {

// 玩家配队槽位: 一行为配队中的一个槽位, 与 InitialTeamTags 顺序一一对应.
// slot_index 表达配队顺序 (0-based), is_active 表达当前上场 (对应 InitialActiveCharacterIndex).
struct TeamSlot {
    int32_t slot_index = 0;
    std::string character_tag;
    bool is_active = false;
    int64_t updated_at = 0;
};

} // namespace data
} // namespace game
