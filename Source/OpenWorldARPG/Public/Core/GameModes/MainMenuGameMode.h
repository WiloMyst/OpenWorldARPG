// Copyright 2025 WilloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "MainMenuGameMode.generated.h"

/**
 * 主菜单 GameMode。
 * 职责：仅作为关卡配置数据（目标关卡）的载体。
 * 切图流转由 UGameFlowSubsystem 统筹，GameMode 不再参与流程调度。
 *
 * 注：初始存档数据（UInitialArchiveData）已迁移至 UOpenWorldARPGSettings，
 *     由 GameFlowSubsystem 自行加载，GameMode 不再持有。
 */
UCLASS()
class OPENWORLDARPG_API AMainMenuGameMode : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Game Mode|Config")
    TSoftObjectPtr<UWorld> GetTargetLevelToLoad() const { return TargetLevelToLoad; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UWorld> TargetLevelToLoad;
};
