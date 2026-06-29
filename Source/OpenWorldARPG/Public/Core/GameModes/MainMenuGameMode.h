// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UStartingRosterConfig;

/**
 * 主菜单 GameMode。
 * 职责：仅作为关卡配置数据（初始队伍、目标关卡）的载体。
 * 切图流转由 UGameFlowSubsystem 统筹，GameMode 不再参与流程调度。
 */
UCLASS()
class OPENWORLDARPG_API AMainMenuGameMode : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    /** 获取初始队伍配置 */
    UFUNCTION(BlueprintPure, Category = "Game Mode|Config")
    UStartingRosterConfig* GetStartingRosterConfig() const;

    /** 获取目标关卡软引用 */
    UFUNCTION(BlueprintPure, Category = "Game Mode|Config")
    TSoftObjectPtr<UWorld> GetTargetLevelToLoad() const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UStartingRosterConfig> StartingRosterAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UWorld> TargetLevelToLoad;
};
