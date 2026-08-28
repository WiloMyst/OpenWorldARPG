#include "game/logic/initial_archive_loader.h"

#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace game {
namespace logic {

data::InitialArchive LoadInitialArchive(const std::string& path) {
    data::InitialArchive out;
    try {
        const YAML::Node root = YAML::LoadFile(path);

        if (const YAML::Node& owned = root["initial_owned_characters"]) {
            for (const auto& n : owned) {
                data::OwnedCharacter c;
                c.character_tag = n["character_tag"].as<std::string>("");
                if (c.character_tag.empty()) continue;
                c.level = n["level"].as<int32_t>(1);
                c.exp = n["exp"].as<int64_t>(0);
                c.acquired_time = 0;  // 落库时由存储层填当前时间
                out.owned_characters.push_back(std::move(c));
            }
        }

        const int32_t active_index = root["initial_active_character_index"].as<int32_t>(0);
        if (const YAML::Node& team = root["initial_team_tags"]) {
            int32_t idx = 0;
            for (const auto& tag_node : team) {
                const std::string tag = tag_node.as<std::string>("");
                if (tag.empty()) { ++idx; continue; }

                data::TeamSlot s;
                s.slot_index = idx;
                s.character_tag = tag;
                s.is_active = (idx == active_index);
                out.team_slots.push_back(std::move(s));

                // 拥有列表里对应角色标记 active (配队内上场 index 推导到角色级 active)
                for (auto& oc : out.owned_characters) {
                    if (oc.character_tag == tag) {
                        oc.is_active = s.is_active;
                    }
                }
                ++idx;
            }
        }

        spdlog::info("[InitialArchive] loaded [path={}, owned={}, team={}, active_index={}]",
                     path, out.owned_characters.size(), out.team_slots.size(), active_index);
    } catch (const YAML::Exception& e) {
        spdlog::error("[InitialArchive] load failed [path={}, err={}]", path, e.what());
    }
    return out;
}

} // namespace logic
} // namespace game
