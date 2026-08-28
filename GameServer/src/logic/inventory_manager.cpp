#include "game/logic/inventory_manager.h"
#include "game/infra/time_utils.h"

#include <algorithm>
#include <cstdio>
#include <random>

namespace game {
namespace logic {

namespace {

// 与客户端 CategoryCapacityLimits 一致的分类容量
const std::unordered_map<std::string, int32_t>& CategoryCapacities() {
    static const std::unordered_map<std::string, int32_t> caps = {
        {"weapon", 1000},
        {"artifact", 1500},
        {"material", 9999},
        {"food", 2000},
        {"quest", 100},
    };
    return caps;
}

} // namespace

InventoryManager::InventoryManager(const ItemDatabase* item_db, std::string account)
    : item_db_(item_db), account_(std::move(account)) {}

void InventoryManager::LoadFrom(std::vector<data::InvItem> items) {
    items_ = std::move(items);
}

std::string InventoryManager::NewGuid() {
    // 36 字符 UUID v4: 版本位 4 / 变体位 10x
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    const uint64_t a = dist(rng);
    const uint64_t b = dist(rng);

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
                  static_cast<uint32_t>(a),
                  static_cast<uint16_t>(a >> 32),
                  static_cast<uint16_t>(((a >> 48) & 0x0FFF) | 0x4000),
                  static_cast<uint16_t>((b & 0x3FFF) | 0x8000),
                  static_cast<uint16_t>(b >> 16),
                  static_cast<uint32_t>(b >> 32));
    return buf;
}

data::InvItem* InventoryManager::FindByGuid(const std::string& guid) {
    auto it = std::find_if(items_.begin(), items_.end(),
                           [&](const data::InvItem& i) { return i.guid == guid; });
    return it == items_.end() ? nullptr : &(*it);
}

bool InventoryManager::HasCategorySpace(const std::string& category) const {
    const auto& caps = CategoryCapacities();
    auto cap_it = caps.find(category);
    if (cap_it == caps.end()) return true;

    int32_t used = 0;
    for (const auto& item : items_) {
        const ItemConfig* cfg = item_db_->Find(item.item_id);
        if (cfg && cfg->category == category) ++used;
    }
    return used < cap_it->second;
}

OpResult InventoryManager::Add(int32_t item_id, int32_t amount) {
    const ItemConfig* cfg = item_db_->Find(item_id);
    if (!cfg) return {false, "Unknown item: " + std::to_string(item_id)};
    if (amount <= 0) return {false, "Amount must be positive"};

    if (cfg->stackable) {
        if (!HasCategorySpace(cfg->category)) {
            return {false, "Category full: " + cfg->category};
        }

        // 先合并已有槽位, 再把剩余开新槽
        int32_t remaining = amount;
        for (auto& item : items_) {
            if (item.item_id != item_id || item.count >= cfg->max_stack) continue;
            const int32_t space = cfg->max_stack - item.count;
            const int32_t moved = std::min(space, remaining);
            item.count += moved;
            remaining -= moved;
            if (remaining == 0) return {true, ""};
        }

        while (remaining > 0) {
            const int32_t put = std::min(cfg->max_stack, remaining);
            data::InvItem item;
            item.guid = NewGuid();
            item.item_id = item_id;
            item.count = put;
            item.acquired_time = infra::NowMs();
            items_.push_back(std::move(item));
            remaining -= put;
        }
        return {true, ""};
    }

    // 不可堆叠: 每个实例独立槽位, amount 必须为 1
    if (amount != 1) return {false, "Item is not stackable, amount must be 1"};
    if (!HasCategorySpace(cfg->category)) {
        return {false, "Category full: " + cfg->category};
    }

    data::InvItem item;
    item.guid = NewGuid();
    item.item_id = item_id;
    item.count = 1;
    item.acquired_time = infra::NowMs();
    items_.push_back(std::move(item));
    return {true, ""};
}

OpResult InventoryManager::Remove(const std::string& guid, int32_t amount) {
    data::InvItem* item = FindByGuid(guid);
    if (!item) return {false, "Item not found"};

    if (item->count > 1) {
        if (amount <= 0) return {false, "Amount must be positive"};
        if (amount > item->count) return {false, "Not enough count"};
        item->count -= amount;
        if (item->count <= 0) {
            items_.erase(std::remove_if(items_.begin(), items_.end(),
                                        [&](const data::InvItem& i) { return i.guid == guid; }),
                         items_.end());
        }
        return {true, ""};
    }

    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&](const data::InvItem& i) { return i.guid == guid; }),
                 items_.end());
    return {true, ""};
}

} // namespace logic
} // namespace game
