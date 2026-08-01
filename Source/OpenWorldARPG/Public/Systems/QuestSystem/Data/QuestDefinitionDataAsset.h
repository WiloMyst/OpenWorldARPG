// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "QuestDefinitionDataAsset.generated.h"

class UDialogueGraphDataAsset;

/**
 * 任务定义（策划可编辑的静态数据）。
 *
 * 对标鸣潮/异环任务数据驱动模式：
 * - 任务用 FGameplayTag 唯一标识（如 Quest.Main.Ch001）
 * - 目标列表支持顺序/并发执行
 * - 前置任务 + 等级解锁
 * - 接取/交付对话用软引用
 */
UCLASS(BlueprintType)
class UQuestDefinitionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 标识 ---

    /** 任务唯一标识 Tag（如 Quest.Main.Ch001） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Identity")
    FGameplayTag QuestTag;

    // --- 显示 ---

    /** 任务名称 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Display")
    FText DisplayName;

    /** 任务描述 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Display")
    FText Description;

    /** 任务类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Display")
    EQuestType QuestType = EQuestType::Side;

    // --- 解锁条件 ---

    /** 前置任务 Tag 列表（全部完成才解锁） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Prerequisite")
    TArray<FGameplayTag> PrerequisiteQuestTags;

    /** 需要的玩家等级 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Prerequisite", meta = (ClampMin = "0"))
    int32 RequiredPlayerLevel = 0;

    // --- 目标与奖励 ---

    /** 任务目标列表 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Objective")
    TArray<FQuestObjective> Objectives;

    /** 任务奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Reward")
    TArray<FQuestReward> Rewards;

    // --- 对话联动 ---

    /** 接取任务时播放的对话 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Dialogue")
    TSoftObjectPtr<UDialogueGraphDataAsset> StartDialogue;

    /** 交付任务时播放的对话 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Dialogue")
    TSoftObjectPtr<UDialogueGraphDataAsset> CompleteDialogue;

    // --- 自动接取 ---

    /** 前置满足后是否自动接取（鸣潮主线/纪闻任务为 true） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Config")
    bool bAutoAccept = false;

    // --- UPrimaryDataAsset ---

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("QuestDefinition"), GetFName());
    }
};
