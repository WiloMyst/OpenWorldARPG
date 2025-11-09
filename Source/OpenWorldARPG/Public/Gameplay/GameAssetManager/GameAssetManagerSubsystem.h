// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameAssetManagerSubsystem.generated.h"

class ULoadingScreenWidget;
class UDataTable;
struct FStreamableManager;
struct FStreamableHandle;

// 定义加载的不同阶段
UENUM(BlueprintType)
enum class ELoadingPhase : uint8
{
    None,
    LoadingLevel,
    LoadingTeamAssets,
    Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadComplete);

/**
 * @class UGameAssetManagerSubsystem
 * @brief 游戏资产管理子系统。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API UGameAssetManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * @brief 获取总的加载进度 (0.0 - 1.0)。UI的进度条应该绑定这个函数。
     */
    UFUNCTION(BlueprintPure, Category = "Asset Loading")
    float GetTotalLoadingProgress() const;

    /**
     * @brief 启动关卡加载流程。
     * @param LevelToLoad 要加载的关卡。
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void TryStartAsyncLevelLoading(TSoftObjectPtr<UWorld> LevelToLoad);

    /**
     * @brief 启动队伍角色资源加载流程。
     * @param TeamCharacterTags 要加载的队伍角色GameplayTag。
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags);

    /**
     * @brief 在新关卡加载完成后，清理所有加载状态并移除加载界面。
     * 建议在新关卡的PlayerController的BeginPlay中调用。
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void CleanupAfterLoad();

    /**
     * @brief 仅显示加载界面UI。
     * 通常由 CleanupAfterLoad 调用，但也可在需要时独立使用。
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void ShowLoadingScreen();

    /**
     * @brief 仅隐藏加载界面UI。
     * 通常由 CleanupAfterLoad 调用，但也可在需要时独立使用。
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading")
    void HideLoadingScreen();

    // 当所有加载（包括关卡）都完成后广播
    UPROPERTY(BlueprintAssignable, Category = "Asset Loading")
    FOnLoadComplete OnLoadComplete;

protected:
    // ----- 内部加载逻辑 -----

    // 队伍资产加载阶段
    void OnSingleTeamAssetLoaded(); // 每个小资产加载完成的回调
    void OnAllTeamAssetsLoaded();   // 整个队伍资产阶段完成的回调

    // 关卡加载阶段
    void StartLevelLoading();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Asset Loading")
    void OnLevelLoadCompleted();    // 关卡加载完成的回调

protected:
    // ----- 配置与状态 -----

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

    UPROPERTY()
    TObjectPtr<ULoadingScreenWidget> LoadingScreenInstance;

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UDataTable> CharacterInfoTable;

    // 真正执行流式加载的管理器
    FStreamableManager* StreamableManager;

    // ----- 状态变量 -----

    // 当前加载阶段
    ELoadingPhase CurrentLoadingPhase = ELoadingPhase::None;

    // 加载阶段的权重
    float LevelLoadWeight = 0.2f;     // 关卡加载占20%
    float TeamAssetLoadWeight = 0.8f; // 队伍资产加载占80%

    // 存储本次加载流程的数据
    TArray<FGameplayTag> CurrentTeamToLoad;
    TSoftObjectPtr<UWorld> CurrentLevelToLoad;
    FName TargetLevelName;

    // 用于跟踪多个资产加载的句柄数组
    TArray<TSharedPtr<FStreamableHandle>> TeamAssetLoadHandles;
    int32 TeamAssetLoadNum = 0;
    int32 CompletedAssetLoads = 0;
    int32 FailedAssetLoads = 0;

    // 用于跟踪关卡加载的句柄
    TSharedPtr<FStreamableHandle> LevelLoadHandle;
};