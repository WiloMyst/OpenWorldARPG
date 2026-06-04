// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UStartingRosterConfig;

UCLASS()
class OPENWORLDARPG_API AMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    // 对应蓝图里的 HandleStartGameRequest 自定义事件
    // 在 UI 蓝图里点击“开始游戏”按钮时，调用这个函数
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void HandleStartGameRequest();

protected:
    // 对应蓝图里的红色的 HandleOnLevelAsyncLoaded 事件
    // 必须加 UFUNCTION() 宏，否则无法绑定给动态多播委托 (AddDynamic)
    UFUNCTION()
    void OnLevelPreloadFinished();

protected:
    // --- 配置项：留给蓝图细节面板配置，拒绝硬编码 ---

    // 初始玩家数据配置
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UStartingRosterConfig> StartingRosterAsset;

    // 要加载的进入游戏后的目标关卡
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UWorld> TargetLevelToLoad;
};