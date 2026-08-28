#pragma once
#include <cstdint>
#include <string>

namespace game {
namespace data {

// 玩家拥有角色的内存表示, 与游戏内角色 Tag 一一对应.
// 构成本作"角色拥有/配队/上场"的数据基础: 账户 -> 多角色 -> active 上场.
// is_active 表达当前配队上场角色, 与服务器权威 CombatManager 的 active_character_tag 对齐.
struct OwnedCharacter {
    std::string character_tag;  // 角色 Tag (如 Character.Playable.Stelle), 与 account 组成业务唯一键
    int32_t level = 1;
    int64_t exp = 0;
    bool is_active = false;     // 是否当前上场角色
    int64_t acquired_time = 0;  // 获取时间 epoch ms
};

} // namespace data
} // namespace game