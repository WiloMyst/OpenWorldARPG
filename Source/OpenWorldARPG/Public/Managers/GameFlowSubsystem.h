// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFlowSubsystem.generated.h"

class UStartingRosterConfig;
class UUIManagerSubsystem;

/** 游戏全局状态。FlowManager 用它来追踪当前处于哪个阶段 */
UENUM(BlueprintType)
enum class EGameState : uint8
{
    /** 未初始化 / 主菜单空闲态 */
    Idle,
    /** 正在切图流转中（Loading 界面已弹出，玩家输入已屏蔽） */
    Traveling,
    /** 新关卡已加载完毕，玩家可操控 */
    Playing
};

/**
 * 游戏流程总指挥子系统。
 *
 * - 生命周期贯穿 GameInstance，作为"上帝视角"俯视所有关卡生灭
 * - 统筹 UIManager（Loading 界面）、AssetManager（资源加载）、Travel（切图执行）
 * - GameMode 不再负责切图逻辑，仅在初始化完毕后向 FlowManager "报到"
 *
 * 切图管线：
 *   Request → UI Block → Asset Block → Travel → Handshake → Complete
 */
UCLASS()
class OPENWORLDARPG_API UGameFlowSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 流程入口 ---

    /**
     * 从主菜单发起进入游戏请求。
     * 总指挥接管流程：初始化队伍数据 → UI Block → Asset Block → Travel
     * @param TargetLevel 目标关卡软引用
     * @param RosterConfig 初始队伍配置（包含拥有角色、队伍成员、激活索引）
     */
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void RequestTravelFromMainMenu(TSoftObjectPtr<UWorld> TargetLevel, UStartingRosterConfig* RosterConfig);

    // --- 新关卡报到 ---

    /**
     * 新关卡的 GameMode 在 PostLogin / BeginPlay 中调用此函数报到。
     * FlowManager 收到报到后执行收尾：触发资源清理、关闭 Loading 屏。
     */
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void NotifyNewLevelReady();

    // --- 查询 ---

    UFUNCTION(BlueprintPure, Category = "Game Flow")
    EGameState GetCurrentGameState() const { return CurrentGameState; }

    UFUNCTION(BlueprintPure, Category = "Game Flow")
    bool IsTraveling() const { return CurrentGameState == EGameState::Traveling; }

protected:
    // --- 管线各阶段 ---

    /** 阶段 1：初始化队伍数据（从 StartingRosterConfig） */
    void InitializeTeamData(UStartingRosterConfig* RosterConfig);

    /** 阶段 2：UI Block — 弹出 Loading 屏，屏蔽输入 */
    void BeginUIBlock();

    /** 阶段 3：Asset Block — 启动关卡 + 队伍资源异步加载 */
    void BeginAssetBlock();

    // --- 回调 ---

    UFUNCTION()
    void OnLevelPreloadFinished();

    UFUNCTION()
    void OnAllAssetsLoaded();

    /** 阶段 4：执行切图（ServerTravel / OpenLevel） */
    void ExecuteTravel();

private:
    /** 当前游戏全局状态 */
    EGameState CurrentGameState = EGameState::Idle;

    /** 缓存的目标关卡（Asset Block 阶段使用） */
    TSoftObjectPtr<UWorld> PendingTargetLevel;

    /** 获取第一个本地玩家的 UIManagerSubsystem */
    UUIManagerSubsystem* GetUIManager() const;
};
