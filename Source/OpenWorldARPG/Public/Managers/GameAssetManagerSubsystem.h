// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameAssetManagerSubsystem.generated.h"

class UDataTable;
class UCharacterGeneralDataAsset;
class UUIDataAsset;
struct FStreamableManager;
struct FStreamableHandle;

/** 加载阶段 */
UENUM(BlueprintType)
enum class ELoadingPhase : uint8
{
    None,
    LoadingLevel,
    LoadingTeamAssets,
    Complete
};

// --- 委托 ---

/** 所有加载流程完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadComplete);

/** 关卡预加载完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelAsyncLoaded);

/**
 * 游戏资产管理子系统。中央资产缓存/访问层 + 异步加载调度器。
 */
UCLASS()
class OPENWORLDARPG_API UGameAssetManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 中央资产缓存（首次从 Settings 延迟加载，后续直接返回缓存）---

    /** 角色信息数据表 */
    UDataTable* GetCharacterInfoTable();

    /** 物品数据库数据表 */
    UDataTable* GetItemDatabaseTable();

    /** 背包分类标签页数据表 */
    UDataTable* GetInventoryCategoryTabDataTable();

    /** 角色通用技能数据资产 */
    UCharacterGeneralDataAsset* GetPlayerCharacterGeneralAbilityDataAsset();

    /** UI 映射数据资产 */
    UUIDataAsset* GetUIMapDataAsset();

    // --- 异步加载调度 ---

    /** 获取总加载进度 (0.0 - 1.0)，UI 进度条绑定此函数 */
    UFUNCTION(BlueprintPure, Category = "Asset Loading")
    float GetTotalLoadingProgress() const;

    /** 启动关卡异步加载流程 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void TryStartAsyncLevelLoading(TSoftObjectPtr<UWorld> LevelToLoad);

    /** 启动队伍角色资源异步加载流程 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags);

    /** 加载完成后分帧释放资源句柄，平滑 GC 卡顿 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void CleanupAfterLoad();

    // --- 事件 ---

    UPROPERTY(BlueprintAssignable, Category = "Asset Loading")
    FOnLoadComplete OnLoadComplete;

    UPROPERTY(BlueprintAssignable, Category = "Asset Loading")
    FOnLevelAsyncLoaded OnLevelAsyncLoaded;

protected:
    // --- 内部加载回调 ---

    void OnSingleTeamAssetLoaded();
    void OnAllTeamAssetsLoaded();
    void StartLevelLoading();

    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void OnLevelLoadCompleted();

    // --- 两阶段队伍资产加载 ---

    /**
     * 阶段 2a：异步加载 VisualDataAsset / CombatDataAsset 本身。
     * FCharacterRegistryRow 中的 VisualData / CombatData 是 TSoftObjectPtr，
     * 直接调用 Get() 会触发同步加载（阻塞主线程）。
     * 必须先将它们异步加载完成，才能安全访问内部字段（AbilityMontages 等）。
     */
    void StartDataAssetLoading();

    /** 阶段 2a 完成回调：数据资产加载完毕后，收集内部软引用并启动阶段 2b */
    void OnDataAssetsLoaded();

    /**
     * 阶段 2b：异步加载所有具体资源（Mesh、Montage、Icon 等）。
     * 此时 VisualDataAsset / CombatDataAsset 已在内存中，可以安全遍历其字段。
     */
    void StartInnerAssetLoading();

    // --- 分帧释放 ---

    /** 每帧释放一批 StreamableHandle，避免集中 GC Spike */
    void ReleaseHandlesStaggered();

    /** 分帧释放完毕后的最终清理 */
    void OnStaggeredReleaseComplete();

protected:
    // --- 缓存资产 ---

    UPROPERTY()
    TObjectPtr<UDataTable> CachedCharacterInfoTable;

    UPROPERTY()
    TObjectPtr<UDataTable> CachedItemDatabaseTable;

    UPROPERTY()
    TObjectPtr<UDataTable> CachedInventoryCategoryTabDataTable;

    UPROPERTY()
    TObjectPtr<UCharacterGeneralDataAsset> CachedPlayerCharacterGeneralAbilityDataAsset;

    UPROPERTY()
    TObjectPtr<UUIDataAsset> CachedUIMapDataAsset;

    FStreamableManager* StreamableManager;

    // --- 加载状态 ---

    ELoadingPhase CurrentLoadingPhase = ELoadingPhase::None;

    float LevelLoadWeight = 0.2f;
    float TeamAssetLoadWeight = 0.8f;
    /** 阶段 2a（数据资产加载）在队伍资产阶段中的权重 */
    float DataAssetPhaseWeight = 0.15f;
    /** 阶段 2b（内部资源加载）在队伍资产阶段中的权重 */
    float InnerAssetPhaseWeight = 0.85f;

    TArray<FGameplayTag> CurrentTeamToLoad;
    TSoftObjectPtr<UWorld> CurrentLevelToLoad;
    FName TargetLevelName;

    TArray<TSharedPtr<FStreamableHandle>> TeamAssetLoadHandles;
    int32 TeamAssetLoadNum = 0;
    int32 CompletedAssetLoads = 0;
    int32 FailedAssetLoads = 0;

    /** 阶段 2a：数据资产（VisualDataAsset/CombatDataAsset）异步加载句柄 */
    TSharedPtr<FStreamableHandle> DataAssetLoadHandle;

    TSharedPtr<FStreamableHandle> LevelLoadHandle;

    // --- 分帧释放状态 ---

    /** 待释放的句柄队列（从 TeamAssetLoadHandles 移入，分帧消耗） */
    TArray<TSharedPtr<FStreamableHandle>> PendingReleaseHandles;

    /** 每帧释放的句柄数量（可配置，默认 3） */
    int32 HandlesToReleasePerFrame = 3;

    /** 分帧释放的定时器句柄 */
    FTimerHandle StaggeredReleaseTimerHandle;
};