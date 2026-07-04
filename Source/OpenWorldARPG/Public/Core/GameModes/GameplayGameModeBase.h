// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GameModes/OpenWorldARPGGameModeBase.h"
#include "GameplayTagContainer.h"
#include "GameplayGameModeBase.generated.h"

class AGameplayPlayerState;
class AGameplayPlayerController;

/**
 * 通用玩法 GameMode。服务器端角色生成与队伍管理的唯一工厂。
 */
UCLASS()
class OPENWORLDARPG_API AGameplayGameModeBase : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    AGameplayGameModeBase();
    virtual void PostLogin(APlayerController* NewPlayer) override;

    // --- 队伍管理 ---

    void ApplyPlayerTeamChanges(AGameplayPlayerController* PlayerController, const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex);

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
    // --- 运行时状态 ---

    FTimerHandle CleanupTimerHandle;
};
