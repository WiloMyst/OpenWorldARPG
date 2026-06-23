// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GameModes/GameplayGameModeBase.h"
#include "DungeonGameMode.generated.h"

class ADungeonPlayerController;

/**
 * 副本 GameMode。继承通用玩法逻辑，设置副本专用的 Controller 和 PlayerState。
 * 未来可扩展：副本计时器、波次生成、结算评分等逻辑。
 */
UCLASS()
class OPENWORLDARPG_API ADungeonGameMode : public AGameplayGameModeBase
{
    GENERATED_BODY()

public:
    ADungeonGameMode();
};
