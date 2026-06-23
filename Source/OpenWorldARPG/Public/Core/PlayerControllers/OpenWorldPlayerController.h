// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "OpenWorldPlayerController.generated.h"

/**
 * 大世界 PlayerController。继承通用战斗输入绑定。
 * 未来可扩展：大地图输入映射、滑翔伞、骑乘等大世界特有输入。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldPlayerController : public AGameplayPlayerController
{
    GENERATED_BODY()

public:
    AOpenWorldPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
};
