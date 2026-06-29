// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "GameplayTagContainer.h"
#include "GameplayGameModeBase.generated.h"

class AGameplayPlayerState;

/**
 * 通用玩法 GameMode。负责服务器端角色生成与初始化。
 * 无论在大世界还是副本，角色实体引用存入 PlayerState（Replicated），GameMode 不保留角色 TMap。
 * 大世界/副本专属逻辑在子类中扩展。
 */
UCLASS()
class OPENWORLDARPG_API AGameplayGameModeBase : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    AGameplayGameModeBase();

    // --- 生命周期 ---

    virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
    // --- 初始化流程 ---
    void GeneratePlayerCharacters(APlayerController* PlayerController);
    void CleanupAfterLoad();

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    FName DefaultPlayerStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    float AssetCleanupDelay;

private:
    FTimerHandle CleanupTimerHandle;
};
