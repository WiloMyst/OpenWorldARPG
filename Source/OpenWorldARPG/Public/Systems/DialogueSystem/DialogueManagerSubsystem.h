// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "Systems/DialogueSystem/Data/DialogueDataTypes.h"
#include "DialogueManagerSubsystem.generated.h"

class UDialogueGraphDataAsset;
class USpeakerDefinitionDataAsset;
class ACharacter;

// ============================================================================
// 委托
// ============================================================================

/** 对话开始委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueStartedDelegate, FGameplayTag, DialogueTag, const TArray<FGameplayTag>&, SpeakerTags);

/** 对话结束委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEndedDelegate);

/** 对话节点变更委托（UI 监听此委托刷新显示） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeChangedDelegate, const FDialogueNode&, Node);

/**
 * 对话管理子系统（每玩家）。
 *
 * 【职责】
 * - 管理对话图运行时推进（节点遍历）
 * - 执行对话节点动作（GiveQuest/CompleteQuest/PlayAnimation 等）
 * - 通过委托广播节点变更，驱动 DialogueWidget 更新
 * - 协调 NPC/玩家角色朝向（对话时面向彼此）
 *
 * 【对话流】
 * StartDialogue(Graph) → 获取入口节点 → 广播 OnDialogueNodeChanged
 *   → UI 显示文本/选项 → 玩家点击继续/选择
 * AdvanceToNext() / SelectChoice(Index) → 查找下一节点 → 执行 PreActions
 *   → 广播 OnDialogueNodeChanged → 循环直到 End 节点
 * EndDialogue() → 执行清理 → 广播 OnDialogueEnded
 */
UCLASS()
class UDialogueManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    // --- 对话生命周期 ---

    /**
     * 开始对话。
     * @param DialogueGraph 对话图资产
     * @param InstigatorCharacter 发起对话的角色（玩家）
     * @param NPCActor 对话的 NPC（可选，用于朝向调整）
     */
    void StartDialogue(UDialogueGraphDataAsset* DialogueGraph, ACharacter* InstigatorCharacter, AActor* NPCActor = nullptr);

    /**
     * Phase 3: 按 GameplayTag 异步加载对话图并启动。
     * 供 AgentActionComponent::HandleTriggerCutscene 调用，LLM 决策触发剧情时使用。
     * @param DialogueTag 对话图标识 Tag（如 Dialogue.Main.Ch001.NPC01）
     * @param InstigatorCharacter 发起对话的角色
     * @param NPCActor 对话的 NPC
     */
    void StartDialogueByTag(const FGameplayTag& DialogueTag, ACharacter* InstigatorCharacter, AActor* NPCActor = nullptr);

    /** 结束当前对话 */
    void EndDialogue();

    /** 当前是否在对话中 */
    bool IsInDialogue() const { return CurrentGraph != nullptr; }

    // --- 对话推进 ---

    /**
     * 推进到下一节点（Speech/Action/Condition 节点用）。
     * 由 DialogueWidget 在玩家点击"继续"时调用。
     */
    void AdvanceToNext();

    /**
     * 选择选项（Choice 节点用）。
     * @param ChoiceIndex 选项索引
     */
    void SelectChoice(int32 ChoiceIndex);

    // --- 状态查询 ---

    /** 获取当前节点 */
    const FDialogueNode* GetCurrentNode() const { return CurrentNode; }

    /** 获取当前说话人定义 */
    USpeakerDefinitionDataAsset* GetSpeakerDefinition(const FGameplayTag& SpeakerTag) const;

    // --- 委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueStartedDelegate OnDialogueStarted;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueEndedDelegate OnDialogueEnded;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueNodeChangedDelegate OnDialogueNodeChanged;

protected:
    // --- 内部 ---

    /** 执行节点动作列表 */
    void ExecuteActions(const TArray<FDialogueAction>& Actions);

    /** 推进到指定节点 ID */
    void AdvanceToNode(int32 NodeID);

    /** 评估 Condition 节点并跳转 */
    void EvaluateConditionNode(const FDialogueNode& Node);

    /** 加载说话人定义 */
    void LoadSpeakerDefinitions(const TArray<FGameplayTag>& SpeakerTags);

private:
    // --- 运行时状态 ---

    /** 当前对话图 */
    UPROPERTY()
    TObjectPtr<UDialogueGraphDataAsset> CurrentGraph = nullptr;

    /** 当前节点 */
    const FDialogueNode* CurrentNode = nullptr;

    /** 发起对话的角色 */
    UPROPERTY()
    TWeakObjectPtr<ACharacter> InstigatorCharacter;

    /** 对话 NPC */
    UPROPERTY()
    TWeakObjectPtr<AActor> NPCActor;

    /** 缓存的说话人定义 */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<USpeakerDefinitionDataAsset>> CachedSpeakers;
};
