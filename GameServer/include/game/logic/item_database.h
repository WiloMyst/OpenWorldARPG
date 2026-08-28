#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace game {
namespace logic {

// 物品静态规则表 (items.yaml): 服务器按此做权威校验
// 客户端不可伪造堆叠上限/分类等规则; 表现层 (名称/图标) 在客户端 DT, 两表经 item_id 对应
struct ItemConfig {
    int32_t item_id = 0;
    std::string category;      // weapon/artifact/material/food/quest
    bool stackable = false;
    int32_t max_stack = 9999;
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
