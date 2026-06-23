// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "DungeonPlayerController.generated.h"

/**
 * 副本 PlayerController。继承通用战斗输入绑定。
 * 未来可扩展：副本专属 UI、屏蔽大世界传送、副本结算等逻辑。
 */
UCLASS()
class OPENWORLDARPG_API ADungeonPlayerController : public AGameplayPlayerController
{
    GENERATED_BODY()

public:
    ADungeonPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
};
