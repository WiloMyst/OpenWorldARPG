#pragma once
#include <vector>

#include "game/data/owned_character.h"
#include "game/data/team_slot.h"

namespace game {
namespace data {

// 新玩家初始存档 (镜像客户端 UInitialArchiveData):
//   owned_characters 拥有角色全集 (InitialOwnedCharacters)
//   team_slots       配队列表 + 上场 index (InitialTeamTags + InitialActiveCharacterIndex)
// 新玩家首次登录由服务器直接落库到 owned_characters / team_slots 两张表.
struct InitialArchive {
    std::vector<OwnedCharacter> owned_characters;
    std::vector<TeamSlot> team_slots;
};

} // namespace data
} // namespace game
