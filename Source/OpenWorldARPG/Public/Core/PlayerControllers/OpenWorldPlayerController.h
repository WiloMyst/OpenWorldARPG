// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "GameplayTagContainer.h"
#include "OpenWorldPlayerController.generated.h"

class UInputAction;

/**
 * 大世界 PlayerController。继承通用战斗输入绑定。
 * 专属职责：
 *   - 钩索 (Hook) 输入派发
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

    // --- 大世界专属输入回调 ---

    /** 钩索输入：向角色发送 HookStartEventTag 事件 */
    void Input_Hook();

protected:
    // --- 配置：大世界专属输入资产 ---

    /** 钩索输入动作 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|OpenWorld")
    TObjectPtr<UInputAction> IA_Hook;

    // --- 配置：大世界专属 Tags ---

    /** 钩索启动事件 Tag */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|OpenWorld")
    FGameplayTag HookStartEventTag;
};
