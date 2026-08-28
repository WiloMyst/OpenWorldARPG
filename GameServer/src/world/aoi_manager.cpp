#include "game/world/aoi_manager.h"
#include <cmath>

namespace game {
namespace world {

AoiManager::AoiManager(float grid_size, int view_radius_cells)
    : grid_size_(grid_size > 0.0f ? grid_size : 1.0f),
      view_radius_cells_(view_radius_cells > 0 ? view_radius_cells : 1) {}

AoiManager::Cell AoiManager::CellOf(float x, float y) const {
    Cell c;
    c.gx = static_cast<int32_t>(std::floor(x / grid_size_));
    c.gy = static_cast<int32_t>(std::floor(y / grid_size_));
    return c;
}

void AoiManager::CollectView(const Cell& c, std::vector<std::string>& out) const {
    for (int32_t dx = -view_radius_cells_; dx <= view_radius_cells_; ++dx) {
        for (int32_t dy = -view_radius_cells_; dy <= view_radius_cells_; ++dy) {
            const Cell n{c.gx + dx, c.gy + dy};
            auto it = cell_entities_.find(n);
            if (it == cell_entities_.end()) continue;
            for (const auto& id : it->second) out.push_back(id);
        }
    }
}

std::vector<std::string> AoiManager::AddEntity(const std::string& id, float x, float y) {
    const Cell c = CellOf(x, y);
    entity_cell_[id] = c;
    cell_entities_[c].insert(id);

    std::vector<std::string> entered;
    CollectView(c, entered);
    return entered;
}

AoiManager::MoveResult AoiManager::MoveEntity(const std::string& id, float x, float y) {
    MoveResult result;
    auto it = entity_cell_.find(id);
    if (it == entity_cell_.end()) {
        result.entered = AddEntity(id, x, y);
        return result;
    }

    const Cell old_cell = it->second;
    const Cell new_cell = CellOf(x, y);
    if (old_cell == new_cell) {
        CollectView(old_cell, result.stayed);
        return result;
    }

    it->second = new_cell;
    auto& old_set = cell_entities_[old_cell];
    old_set.erase(id);
    if (old_set.empty()) cell_entities_.erase(old_cell);
    cell_entities_[new_cell].insert(id);

    std::vector<std::string> old_view, new_view;
    CollectView(old_cell, old_view);
    CollectView(new_cell, new_view);

    std::unordered_set<std::string> old_view_set(old_view.begin(), old_view.end());
    std::unordered_set<std::string> new_view_set(new_view.begin(), new_view.end());
    for (const auto& s : old_view) {
        if (new_view_set.count(s) == 0) {
            result.left.push_back(s);
        } else {
            result.stayed.push_back(s);
        }
    }
    for (const auto& s : new_view) {
        if (old_view_set.count(s) == 0) result.entered.push_back(s);
    }
    return result;
}

std::vector<std::string> AoiManager::RemoveEntity(const std::string& id) {
    std::vector<std::string> left;
    auto it = entity_cell_.find(id);
    if (it == entity_cell_.end()) return left;

    CollectView(it->second, left);
    auto& set = cell_entities_[it->second];
    set.erase(id);
    if (set.empty()) cell_entities_.erase(it->second);
    entity_cell_.erase(it);
    return left;
}

bool AoiManager::Contains(const std::string& id) const {
    return entity_cell_.find(id) != entity_cell_.end();
}

} // namespace world
} // namespace game
