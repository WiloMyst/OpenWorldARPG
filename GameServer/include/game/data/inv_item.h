#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace game {
namespace data {

// 背包物品的内存表示, 与 proto ItemInstance 一一对应
// ext 数据 (武器成长/圣遗物词条) 以 JSON 持久化在 inventory_items.ext_json
struct ArtifactSubStat {
    std::string stat_type;
    float stat_value = 0.f;
    int32_t upgrade_count = 0;
};

struct InvItem {
    std::string guid;
    int32_t item_id = 0;
    int32_t count = 0;
    int32_t equipped_character_id = -1;
    int64_t acquired_time = 0;

    bool has_weapon = false;
    int32_t weapon_level = 1;
    int32_t weapon_ascension = 0;
    int32_t weapon_refinement = 1;

    bool has_artifact = false;
    int32_t artifact_set_id = 0;
    std::string artifact_slot;
    std::string artifact_main_stat;
    float artifact_main_stat_value = 0.f;
    std::vector<ArtifactSubStat> artifact_sub_stats;
};

} // namespace data
} // namespace game
