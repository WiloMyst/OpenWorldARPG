// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DialogueDataTypes.generated.h"

class UAnimMontage;

// ============================================================================
// 对话枚举
// ============================================================================

/** 对话节点类型 */
UENUM(BlueprintType)
enum class EDialogueNodeType : uint8
{
    Speech      UMETA(DisplayName = "说话"),
    Choice      UMETA(DisplayName = "选项"),
    Condition   UMETA(DisplayName = "条件分支"),
    Action      UMETA(DisplayName = "动作触发"),
    End         UMETA(DisplayName = "结束"),
};

/** 说话情绪（驱动头像选择和动画） */
UENUM(BlueprintType)
enum class EDialogueEmotion : uint8
{
    Neutral     UMETA(DisplayName = "中性"),
    Happy       UMETA(DisplayName = "开心"),
    Angry       UMETA(DisplayName = "愤怒"),
    Sad         UMETA(DisplayName = "悲伤"),
    Surprised   UMETA(DisplayName = "惊讶"),
};

/** 对话动作类型（联动任务/战斗/动画） */
UENUM(BlueprintType)
enum class EDialogueActionType : uint8
{
    None                UMETA(DisplayName = "无"),
    GiveQuest           UMETA(DisplayName = "给予任务"),
    CompleteQuest       UMETA(DisplayName = "完成任务"),
    AdvanceObjective    UMETA(DisplayName = "推进任务目标"),
    PlayAnimation       UMETA(DisplayName = "播放动画"),
    GiveItem            UMETA(DisplayName = "给予物品"),
    StartCombat         UMETA(DisplayName = "开始战斗"),
    OpenShop            UMETA(DisplayName = "打开商店"),
};

// ============================================================================
// 对话结构体
// ============================================================================

/** 对话动作（挂在对话节点上，节点进入/退出时执行） */
USTRUCT(BlueprintType)
struct FDialogueAction
{
    GENERATED_BODY()

    /** 动作类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    EDialogueActionType Type = EDialogueActionType::None;

    /** 关联的 Quest Tag（GiveQuest/CompleteQuest/AdvanceObjective 用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "Type == EDialogueActionType::GiveQuest || Type == EDialogueActionType::CompleteQuest || Type == EDialogueActionType::AdvanceObjective"))
    FGameplayTag QuestTag;

    /** 关联的任务目标 Tag（AdvanceObjective 用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "Type == EDialogueActionType::AdvanceObjective"))
    FGameplayTag ObjectiveTag;

    /** 关联的物品 ID（GiveItem 用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "Type == EDialogueActionType::GiveItem"))
    FName ItemID = NAME_None;

    /** 关联的动画蒙太奇（PlayAnimation 用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "Type == EDialogueActionType::PlayAnimation"))
    TSoftObjectPtr<UAnimMontage> AnimationMontage;
};

/** 对话选项 */
USTRUCT(BlueprintType)
struct FDialogueChoice
{
    GENERATED_BODY()

    /** 选项显示文本 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    FText ChoiceText;

    /** 选择此选项后跳转的节点 ID（-1 = 结束对话） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    int32 TargetNodeID = -1;

    /** 选择此选项时触发的动作 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TArray<FDialogueAction> Actions;

    /** 条件 Tag（玩家需持有此 Tag 选项才可选，空 = 无条件） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    FGameplayTag RequiredTag;
};

/** 对话节点（EdGraph 数据节点的运行时表示） */
USTRUCT(BlueprintType)
struct FDialogueNode
{
    GENERATED_BODY()

    /** 节点唯一 ID（图内唯一，用于跳转引用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    int32 NodeID = 0;

    /** 节点类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    EDialogueNodeType NodeType = EDialogueNodeType::Speech;

    // --- Speech 节点字段 ---

    /** 说话人 Tag（关联 USpeakerDefinitionDataAsset） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Speech"))
    FGameplayTag SpeakerTag;

    /** 说话文本 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Speech || NodeType == EDialogueNodeType::Choice", MultiLine = true))
    FText SpeechText;

    /** 说话情绪 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Speech"))
    EDialogueEmotion Emotion = EDialogueEmotion::Neutral;

    /** 语音事件 Tag（空 = 无语音） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Speech"))
    FGameplayTag VoiceCue;

    // --- 流转 ---

    /** 下一节点 ID（-1 = 结束对话，Speech/Action/Condition 节点用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType != EDialogueNodeType::Choice && NodeType != EDialogueNodeType::End"))
    int32 NextNodeID = -1;

    /** 选项列表（Choice 节点用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Choice"))
    TArray<FDialogueChoice> Choices;

    // --- 动作 ---

    /** 节点进入前执行的动作 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TArray<FDialogueAction> PreActions;

    /** 节点结束后执行的动作 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TArray<FDialogueAction> PostActions;

    // --- Condition 节点字段 ---

    /** 条件 Tag（玩家需持有此 Tag 才走 NextNodeID，否则走 FallbackNodeID） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Condition"))
    FGameplayTag ConditionTag;

    /** 条件不满足时跳转的节点 ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "NodeType == EDialogueNodeType::Condition"))
    int32 FallbackNodeID = -1;
};
