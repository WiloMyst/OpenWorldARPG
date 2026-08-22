#include "game/logic/item_database.h"
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace game {
namespace logic {

ItemDatabase ItemDatabase::LoadFromYaml(const std::string& path) {
    ItemDatabase db;
    YAML::Node root = YAML::LoadFile(path);

    const YAML::Node& items = root["items"];
    if (!items || !items.IsSequence()) {
        spdlog::critical("[ItemDatabase] 'items' sequence missing in {}", path);
        throw std::runtime_error("ItemDatabase load failed");
    }

    for (const auto& node : items) {
        ItemConfig cfg;
        cfg.item_id = node["item_id"].as<int32_t>();
        cfg.category = node["category"].as<std::string>();
        cfg.stackable = node["stackable"].as<bool>(false);
        cfg.max_stack = node["max_stack"].as<int32_t>(9999);
        cfg.use_target = node["use_target"].as<std::string>("none");

        if (cfg.category == "artifact") {
            const YAML::Node& art = node["artifact"];
            if (!art) {
                spdlog::critical("[ItemDatabase] artifact item {} missing 'artifact' section", cfg.item_id);
                throw std::runtime_error("ItemDatabase load failed");
            }
            cfg.has_artifact = true;
            cfg.artifact_set_id = art["set_id"].as<int32_t>(0);
            cfg.artifact_slot = art["slot"].as<std::string>();
            cfg.artifact_main_stat = art["main_stat"].as<std::string>();
            cfg.artifact_main_stat_base = art["main_stat_base"].as<float>(0.f);
        }

        db.items_.emplace(cfg.item_id, std::move(cfg));
    }

    spdlog::info("[ItemDatabase] Loaded {} item configs", db.items_.size());
    return db;
}

const ItemConfig* ItemDatabase::Find(int32_t item_id) const {
    auto it = items_.find(item_id);
    return it == items_.end() ? nullptr : &it->second;
}

} // namespace logic
} // namespace game
