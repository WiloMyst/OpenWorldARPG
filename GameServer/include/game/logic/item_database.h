#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace game {
namespace logic {

// 物品静态配置, 对应客户端 DT_ItemDatabase 的服务器侧镜像
// 服务器按此配置做权威校验, 客户端不可伪造堆叠上限/分类等规则
struct ItemConfig {
    int32_t item_id = 0;
    std::string category;      // weapon/artifact/material/food/quest
    bool stackable = false;
    int32_t max_stack = 9999;
    std::string use_target;    // none/self/select_character/world

    // 圣遗物静态属性 (发放时生成实例数据)
    bool has_artifact = false;
    int32_t artifact_set_id = 0;
    std::string artifact_slot;
    std::string artifact_main_stat;
    float artifact_main_stat_base = 0.f;
};

class ItemDatabase {
public:
    static ItemDatabase LoadFromYaml(const std::string& path);

    // 未配置返回 nullptr
    const ItemConfig* Find(int32_t item_id) const;

private:
    std::unordered_map<int32_t, ItemConfig> items_;
};

} // namespace logic
} // namespace game
