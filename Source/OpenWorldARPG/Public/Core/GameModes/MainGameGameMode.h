// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "GameplayTagContainer.h"
#include "MainGameGameMode.generated.h"

class APlayerCharacter;
class AMainGamePlayerState;

/**
 * 主游戏 GameMode。负责服务器端角色生成与初始化。
 * 角色实体引用存入 PlayerState（Replicated），GameMode 不保留角色 TMap。
 */
UCLASS()
class OPENWORLDARPG_API AMainGameGameMode : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    AMainGameGameMode();

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
    TSubclassOf<APlayerCharacter> PlayerCharacterClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    float AssetCleanupDelay;

private:
    FTimerHandle CleanupTimerHandle;
};
