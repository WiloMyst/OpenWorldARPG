// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameStates/OpenWorldARPGGameStateBase.h"
#include "Systems/CharacterManager/ServerPlayerDataManager.h"

AOpenWorldARPGGameStateBase::AOpenWorldARPGGameStateBase()
{
    // 服务器侧数据管理组件。仅服务器使用，但组件本身需在所有端创建（组件不复制其内部数据）。
    ServerPlayerDataManager = CreateDefaultSubobject<UServerPlayerDataManager>(TEXT("ServerPlayerDataManager"));
}
