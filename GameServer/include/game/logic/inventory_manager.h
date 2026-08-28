#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game/data/inv_item.h"
#include "game/logic/item_database.h"

namespace game {
namespace logic {

// 操作结果: 失败时 reason 给出可展示的错误文案
struct OpResult {
    bool ok = false;
    std::string reason;
};

// 权威背包: 内存态 + 全部规则校验, 每次成功操作由 GameLogic 负责落库
// 业务范围: 拾取/丢弃. 规则对齐客户端 InventoryManagerSubsystem: 堆叠合并/分类容量
// 线程安全由 GameLogic 持锁保证, 本类不做内部加锁
class InventoryManager {
public:
    InventoryManager(const ItemDatabase* item_db, std::string account);

    // 登录时从 DB 加载
    void LoadFrom(std::vector<data::InvItem> items);

    // ---- 权威操作 ----
    // 可堆叠: 同 item_id 槽位按 max_stack 合并, 溢出开新槽
    // 不可堆叠: 每实例独立槽位, amount 必须为 1
    OpResult Add(int32_t item_id, int32_t amount);
    // 堆叠物品扣减数量, 不可堆叠物品整实例移除
    OpResult Remove(const std::string& guid, int32_t amount);

    const std::vector<data::InvItem>& items() const { return items_; }

private:
    data::InvItem* FindByGuid(const std::string& guid);
    bool HasCategorySpace(const std::string& category) const;
    static std::string NewGuid();

    const ItemDatabase* item_db_;
    std::string account_;
    std::vector<data::InvItem> items_;
};

} // namespace logic
} // namespace game
