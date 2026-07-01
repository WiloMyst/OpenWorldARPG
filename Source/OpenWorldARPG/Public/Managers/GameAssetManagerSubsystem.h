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

UENUM(BlueprintType)
enum class ELoadingPhase : uint8
{
    None,
    LoadingLevel,
    LoadingTeamAssets,
    Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadComplete);
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

    // --- 资产缓存访问 ---

    UDataTable* GetCharacterInfoTable();
    UDataTable* GetItemDatabaseTable();
    UDataTable* GetInventoryCategoryTabDataTable();
    UCharacterGeneralDataAsset* GetPlayerCharacterGeneralAbilityDataAsset();
    UUIDataAsset* GetUIMapDataAsset();

    // --- 异步加载调度 ---

    float GetTotalLoadingProgress() const;
    void TryStartAsyncLevelLoading(TSoftObjectPtr<UWorld> LevelToLoad);
    void StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags);

    // --- 清理 ---

    void CleanupAfterLoad();

protected:
    // --- 加载回调 ---

    void OnSingleTeamAssetLoaded();
    void OnAllTeamAssetsLoaded();
    void StartLevelLoading();
    void OnLevelLoadCompleted();

    // --- 两阶段队伍资产加载 ---

    void StartDataAssetLoading();
    void OnDataAssetsLoaded();
    void StartInnerAssetLoading();

    // --- 分帧释放 ---

    void ReleaseHandlesStaggered();
    void OnStaggeredReleaseComplete();

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Asset Loading")
    FOnLoadComplete OnLoadComplete;

    UPROPERTY(BlueprintAssignable, Category = "Asset Loading")
    FOnLevelAsyncLoaded OnLevelAsyncLoaded;

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

    // --- StreamableManager ---

    FStreamableManager* StreamableManager;

    // --- 加载状态 ---

    ELoadingPhase CurrentLoadingPhase = ELoadingPhase::None;

    TArray<FGameplayTag> CurrentTeamToLoad;
    TSoftObjectPtr<UWorld> CurrentLevelToLoad;
    FName TargetLevelName;

    TArray<TSharedPtr<FStreamableHandle>> TeamAssetLoadHandles;
    TSharedPtr<FStreamableHandle> DataAssetLoadHandle;
    TSharedPtr<FStreamableHandle> LevelLoadHandle;

    int32 TeamAssetLoadNum = 0;
    int32 CompletedAssetLoads = 0;
    int32 FailedAssetLoads = 0;

    // --- 加载权重配置 ---

    float LevelLoadWeight = 0.2f;
    float TeamAssetLoadWeight = 0.8f;
    float DataAssetPhaseWeight = 0.15f;
    float InnerAssetPhaseWeight = 0.85f;

    // --- 分帧释放状态 ---

    TArray<TSharedPtr<FStreamableHandle>> PendingReleaseHandles;
    int32 HandlesToReleasePerFrame = 3;
    FTimerHandle StaggeredReleaseTimerHandle;
};
