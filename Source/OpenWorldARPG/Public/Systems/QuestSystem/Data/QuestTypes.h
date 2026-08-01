// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.generated.h"

// ============================================================================
// 任务枚举
// ============================================================================

/** 任务类型（对标鸣潮 6 分类） */
UENUM(BlueprintType)
enum class EQuestType : uint8
{
    Main        UMETA(DisplayName = "主线任务"),
    Companion   UMETA(DisplayName = "伴星任务"),
    Guide       UMETA(DisplayName = "道引任务"),
    Side        UMETA(DisplayName = "支线任务"),
    Daily       UMETA(DisplayName = "日常任务"),
};

/** 任务状态 */
UENUM(BlueprintType)
enum class EQuestState : uint8
{
    Locked          UMETA(DisplayName = "未解锁"),
    Available       UMETA(DisplayName = "可接取"),
    Active          UMETA(DisplayName = "进行中"),
    ReadyToTurnIn   UMETA(DisplayName = "可交付"),
    Completed       UMETA(DisplayName = "已完成"),
    Failed          UMETA(DisplayName = "已失败"),
};

/** 任务目标类型 */
UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
    ReachLocation   UMETA(DisplayName = "到达位置"),
    KillN            UMETA(DisplayName = "击杀N个"),
    CollectN         UMETA(DisplayName = "采集N个"),
    TalkToNPC        UMETA(DisplayName = "与NPC对话"),
    UseItem          UMETA(DisplayName = "使用物品"),
    CompleteCombat   UMETA(DisplayName = "完成战斗副本"),
    TriggerEvent     UMETA(DisplayName = "触发事件"),
};

// ============================================================================
// 任务结构体
// ============================================================================

/** 单个任务目标定义 */
USTRUCT(BlueprintType)
struct FQuestObjective
{
    GENERATED_BODY()

    /** 目标类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    EQuestObjectiveType Type = EQuestObjectiveType::TalkToNPC;

    /** 目标标识 Tag（如 Quest.Objective.Kill.Boss01） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FGameplayTag ObjectiveTag;

    /** 需要的数量（击杀/采集类） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "1"))
    int32 RequiredCount = 1;

    /** 目标位置（到达位置类） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::ReachLocation"))
    FVector TargetLocation = FVector::ZeroVector;

    /** 目标 NPC Tag（对话类） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::TalkToNPC"))
    FGameplayTag TargetNPCTag;

    /** UI 显示文本 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText DisplayText;

    /** 是否可选目标 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    bool bOptional = false;
};

/** 任务奖励 */
USTRUCT(BlueprintType)
struct FQuestReward
{
    GENERATED_BODY()

    /** 物品 ID（关联 DT_ItemDatabase） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
    FName ItemID = NAME_None;

    /** 物品数量 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
    int32 Quantity = 1;

    /** 经验奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
    int32 Experience = 0;

    /** 货币奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
    int32 Currency = 0;
};

/** 任务运行时进度（存档用） */
USTRUCT(BlueprintType)
struct FQuestProgress
{
    GENERATED_BODY()

    /** 当前状态 */
    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    EQuestState State = EQuestState::Locked;

    /** 各目标的当前完成计数 */
    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    TMap<FGameplayTag, int32> ObjectiveCounts;

    /** 接取时间（日常任务重置用） */
    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    FDateTime AcceptTime = FDateTime(0);

    /** 是否正在追踪 */
    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    bool bTracked = false;
};

/** 任务状态变更事件载荷 */
USTRUCT(BlueprintType)
struct FQuestStateChangedPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    FGameplayTag QuestTag;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    EQuestState OldState = EQuestState::Locked;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    EQuestState NewState = EQuestState::Locked;
};
