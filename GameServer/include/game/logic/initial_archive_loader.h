#pragma once
#include <string>

#include "game/data/initial_archive.h"

namespace game {
namespace logic {

// 加载新玩家初始存档配置 (initial_archive.yaml), 镜像客户端 UInitialArchiveData.
// 拥有列表与配队列表分开加载; is_active 由配队内 active index 推导到对应角色.
// 解析失败返回空结构 (调用方按"无初始存档"处理, 不阻断启动).
data::InitialArchive LoadInitialArchive(const std::string& path);

} // namespace logic
} // namespace game
