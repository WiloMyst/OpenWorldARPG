// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameAssetManagerSubsystem.generated.h"

class ULoadingScreenWidget;
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
 * @class UGameAssetManagerSubsystem
 * @brief 游戏资产管理子系统。中央资产缓存/访问层 + 异步加载调度器。
 * 所有资产路径统一从 UOpenWorldARPGSettings 获取，不硬编码。
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

    /** 角色通用技能数据资产 */
    UCharacterGeneralDataAsset* GetPlayerCharacterGeneralAbilityDataAsset();

    /** UI 映射数据资产 */
    UUIDataAsset* GetUIMapDataAsset();

    /** 加载界面 Widget 类 */
    TSubclassOf<ULoadingScreenWidget> GetLoadingScreenWidgetClass() const;

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

    /** 加载完成后清理状态并移除加载界面 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void CleanupAfterLoad();

    /** 显示加载界面 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void ShowLoadingScreen();

    /** 隐藏加载界面 */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void HideLoadingScreen();

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

protected:
    UPROPERTY()
    TObjectPtr<ULoadingScreenWidget> LoadingScreenInstance;

    // --- 缓存资产 ---

    UPROPERTY()
    TObjectPtr<UDataTable> CachedCharacterInfoTable;

    UPROPERTY()
    TObjectPtr<UDataTable> CachedItemDatabaseTable;

    UPROPERTY()
    TObjectPtr<UCharacterGeneralDataAsset> CachedPlayerCharacterGeneralAbilityDataAsset;

    UPROPERTY()
    TObjectPtr<UUIDataAsset> CachedUIMapDataAsset;

    FStreamableManager* StreamableManager;

    // --- 加载状态 ---

    ELoadingPhase CurrentLoadingPhase = ELoadingPhase::None;

    float LevelLoadWeight = 0.2f;
    float TeamAssetLoadWeight = 0.8f;

    TArray<FGameplayTag> CurrentTeamToLoad;
    TSoftObjectPtr<UWorld> CurrentLevelToLoad;
    FName TargetLevelName;

    TArray<TSharedPtr<FStreamableHandle>> TeamAssetLoadHandles;
    int32 TeamAssetLoadNum = 0;
    int32 CompletedAssetLoads = 0;
    int32 FailedAssetLoads = 0;

    TSharedPtr<FStreamableHandle> LevelLoadHandle;
};