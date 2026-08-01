// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "Systems/QuestSystem/Data/QuestTypes.h"
#include "QuestManagerSubsystem.generated.h"

class UQuestDefinitionDataAsset;

// ============================================================================
// 委托
// ============================================================================

/** 任务状态变更委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStateChangedDelegate, const FQuestStateChangedPayload&, Payload);

/** 任务目标进度变更委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestObjectiveProgressDelegate,
    FGameplayTag, QuestTag, FGameplayTag, ObjectiveTag, int32, CurrentCount);

/**
 * 任务管理子系统（每玩家）。
 *
 * 【职责】
 * - 管理玩家所有任务的运行时状态（状态机）
 * - 提供接取/推进/完成/失败 API
 * - 通过委托广播状态变更，驱动 UI/小地图更新
 * - 提供存档读写接口（实际持久化由 ServerPlayerDataManager 完成）
 *
 * 【架构对标】
 * - 继承 ULocalPlayerSubsystem（与 InventoryManagerSubsystem 一致）
 * - 任务定义用 UQuestDefinitionDataAsset（数据驱动）
 * - 状态用 EQuestState 枚举 + FGameplayTag 双重标识
 * - 目标完成上报通过 AdvanceObjective API（供 GAS Event / Dialogue Action 调用）
 */
UCLASS()
class UQuestManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    // --- 生命周期 ---

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 任务状态查询 ---

    /** 获取任务当前状态 */
    EQuestState GetQuestState(const FGameplayTag& QuestTag) const;

    /** 任务是否已完成 */
    bool IsQuestCompleted(const FGameplayTag& QuestTag) const;

    /** 任务是否处于指定状态 */
    bool IsQuestInState(const FGameplayTag& QuestTag, EQuestState State) const;

    /** 获取所有处于指定状态的任务 Tag */
    void GetQuestsByState(EQuestState State, TArray<FGameplayTag>& OutQuestTags) const;

    /** 获取当前追踪的任务 */
    FGameplayTag GetTrackedQuest() const { return TrackedQuestTag; }

    // --- 任务操作 ---

    /**
     * 接取任务。
     * @param QuestTag 任务标识
     * @return 是否接取成功（前置满足 + 状态为 Available/Locked）
     */
    bool AcceptQuest(const FGameplayTag& QuestTag);

    /**
     * 推进任务目标进度。
     * @param QuestTag 任务标识
     * @param ObjectiveTag 目标标识
     * @param DeltaCount 增量（默认 +1）
     * @return 推进后的当前计数（-1 = 失败，任务不存在或非 Active）
     */
    int32 AdvanceObjective(const FGameplayTag& QuestTag, const FGameplayTag& ObjectiveTag, int32 DeltaCount = 1);

    /**
     * 交付任务。
     * 检查所有必选目标是否完成，完成后状态转为 Completed。
     */
    bool TurnInQuest(const FGameplayTag& QuestTag);

    /** 任务失败 */
    void FailQuest(const FGameplayTag& QuestTag);

    /** 设置追踪任务（屏幕指引/小地图标记） */
    void SetTrackedQuest(const FGameplayTag& QuestTag);

    // --- 存档接口 ---

    /** 从存档数据加载任务进度（由 ServerPlayerDataManager 调用） */
    void LoadFromSaveData(const TMap<FGameplayTag, FQuestProgress>& InQuestProgressMap);

    /** 收集任务进度到存档数据（由 ServerPlayerDataManager 调用） */
    void CollectSaveData(TMap<FGameplayTag, FQuestProgress>& OutQuestProgressMap) const;

    // --- 任务定义查询 ---

    /** 异步加载任务定义 */
    void LoadQuestDefinition(const FGameplayTag& QuestTag, TFunction<void(UQuestDefinitionDataAsset*)> Callback);

    // --- 委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestStateChangedDelegate OnQuestStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestObjectiveProgressDelegate OnQuestObjectiveProgress;

protected:
    // --- 内部 ---

    /** 检查任务前置条件是否满足 */
    bool ArePrerequisitesMet(const FGameplayTag& QuestTag) const;

    /** 检查任务所有必选目标是否完成 */
    bool AreAllObjectivesCompleted(const FGameplayTag& QuestTag) const;

    /** 设置任务状态（内部，含委托广播） */
    void SetQuestState(const FGameplayTag& QuestTag, EQuestState NewState);

    /** 获取任务定义（已缓存的） */
    UQuestDefinitionDataAsset* GetCachedDefinition(const FGameplayTag& QuestTag) const;

private:
    // --- 运行时状态 ---

    /** 任务进度表：QuestTag -> Progress */
    UPROPERTY()
    TMap<FGameplayTag, FQuestProgress> QuestProgressMap;

    /** 缓存的任务定义 */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UQuestDefinitionDataAsset>> CachedDefinitions;

    /** 当前追踪的任务 Tag */
    FGameplayTag TrackedQuestTag;
};
