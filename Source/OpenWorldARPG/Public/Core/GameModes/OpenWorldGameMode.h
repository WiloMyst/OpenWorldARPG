// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GameModes/GameplayGameModeBase.h"
#include "OpenWorldGameMode.generated.h"

class AOpenWorldPlayerController;

/**
 * 大世界 GameMode。继承通用玩法逻辑，设置大世界专用的 Controller 和 PlayerState。
 * 未来可扩展：大世界天气系统、传送点、世界 Boss 等逻辑。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldGameMode : public AGameplayGameModeBase
{
    GENERATED_BODY()

public:
    AOpenWorldGameMode();
};
