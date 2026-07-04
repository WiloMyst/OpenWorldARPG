// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFlowSubsystem.generated.h"

class UUIManagerSubsystem;

/** 游戏全局状态 */
UENUM(BlueprintType)
enum class EGameState : uint8
{
    Idle,       // 主菜单空闲态
    Traveling,  // 切图流转中（Loading 已弹出，输入已屏蔽）
    Playing     // 关卡已就绪，玩家可操控
};

/**
 * 游戏流程总指挥子系统。
 * 统筹 UI Block → Asset Block → Travel → Handshake 全流程，
 * GameMode 仅在初始化完毕后报到，不负责切图逻辑。
 */
UCLASS()
class OPENWORLDARPG_API UGameFlowSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 流程入口 ---

    /** 从主菜单发起进入游戏请求。存档数据从 UOpenWorldARPGSettings 获取 */
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void RequestTravelFromMainMenu(TSoftObjectPtr<UWorld> TargetLevel);

    // --- 新关卡报到 ---

    /** 新关卡 GameMode 报到，触发资源清理与关闭 Loading */
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void NotifyNewLevelReady();

    // --- 查询 ---

    UFUNCTION(BlueprintPure, Category = "Game Flow")
    EGameState GetCurrentGameState() const { return CurrentGameState; }

    UFUNCTION(BlueprintPure, Category = "Game Flow")
    bool IsTraveling() const { return CurrentGameState == EGameState::Traveling; }

protected:
    // --- 管线各阶段 ---

    void InitializeTeamData();  // 阶段 1：初始化队伍数据
    void BeginUIBlock();         // 阶段 2：弹出 Loading 屏，屏蔽输入
    void BeginAssetBlock();      // 阶段 3：关卡 + 队伍资源异步加载

    UFUNCTION()
    void OnLevelPreloadFinished();

    UFUNCTION()
    void OnAllAssetsLoaded();

    void ExecuteTravel();       // 阶段 4：执行切图

private:
    EGameState CurrentGameState = EGameState::Idle;
    TSoftObjectPtr<UWorld> PendingTargetLevel;
    UUIManagerSubsystem* GetUIManager() const;
};
