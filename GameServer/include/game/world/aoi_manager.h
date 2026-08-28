#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace game {
namespace world {

// 九宫格 AOI: 以格子管理实体视野
// 格子坐标 = floor(pos / grid_size), 视野 = 周围 (2*view_radius_cells+1)^2 格
// 实体以 string id (账号) 标识; 线程安全由调用方 (WorldManager) 保证
class AoiManager {
public:
    AoiManager(float grid_size, int view_radius_cells);

    // 实体进入世界: 返回视野内实体 (含自身, 调用方过滤)
    std::vector<std::string> AddEntity(const std::string& id, float x, float y);

    struct MoveResult {
        std::vector<std::string> entered;  // 新进入视野 (含自身, 调用方过滤)
        std::vector<std::string> left;     // 离开视野 (含自身, 调用方过滤)
        std::vector<std::string> stayed;   // 仍在视野 (含自身, 调用方过滤)
    };
    MoveResult MoveEntity(const std::string& id, float x, float y);

    // 实体离开世界: 返回原视野内实体 (含自身, 调用方过滤)
    std::vector<std::string> RemoveEntity(const std::string& id);

    bool Contains(const std::string& id) const;

private:
    struct Cell {
        int32_t gx = 0;
        int32_t gy = 0;
        bool operator==(const Cell& o) const { return gx == o.gx && gy == o.gy; }
    };
    struct CellHash {
        size_t operator()(const Cell& c) const {
            return (static_cast<uint64_t>(static_cast<uint32_t>(c.gx)) << 32) ^
                   static_cast<uint32_t>(c.gy);
        }
    };

    Cell CellOf(float x, float y) const;
    void CollectView(const Cell& c, std::vector<std::string>& out) const;

    float grid_size_;
    int view_radius_cells_;
    std::unordered_map<std::string, Cell> entity_cell_;
    std::unordered_map<Cell, std::unordered_set<std::string>, CellHash> cell_entities_;
};

} // namespace world
} // namespace game
