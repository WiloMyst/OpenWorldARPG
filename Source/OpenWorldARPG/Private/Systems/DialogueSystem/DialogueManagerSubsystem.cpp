// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/DialogueSystem/DialogueManagerSubsystem.h"
#include "Systems/DialogueSystem/Data/DialogueGraphDataAsset.h"
#include "Systems/DialogueSystem/Data/SpeakerDefinitionDataAsset.h"
#include "Systems/QuestSystem/QuestManagerSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Engine/AssetManager.h"

// ============================================================================
// 对话生命周期
// ============================================================================

void UDialogueManagerSubsystem::StartDialogue(UDialogueGraphDataAsset* DialogueGraph, ACharacter* InInstigatorCharacter, AActor* InNPCActor)
{
    if (!DialogueGraph || !InInstigatorCharacter) return;
    // 已在对话中则先结束
    if (IsInDialogue())
    {
        EndDialogue();
    }

    CurrentGraph = DialogueGraph;
    InstigatorCharacter = InInstigatorCharacter;
    NPCActor = InNPCActor;

    // 加载说话人定义
    LoadSpeakerDefinitions(DialogueGraph->SpeakerTags);

    // 广播对话开始
    OnDialogueStarted.Broadcast(DialogueGraph->DialogueTag, DialogueGraph->SpeakerTags);

    // 推进到入口节点
    AdvanceToNode(0);
}

void UDialogueManagerSubsystem::StartDialogueByTag(const FGameplayTag& DialogueTag, ACharacter* InInstigatorCharacter, AActor* InNPCActor)
{
    // Phase 3: LLM 决策触发剧情时调用
    // 通过 AssetManager 异步加载 DialogueGraphDataAsset，加载完成后回调 StartDialogue
    if (!DialogueTag.IsValid() || !InInstigatorCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DialogueManager] StartDialogueByTag 参数无效: Tag=%s"), *DialogueTag.ToString());
        return;
    }

    if (!UAssetManager::IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[DialogueManager] AssetManager 未初始化，无法按 Tag 加载对话图"));
        return;
    }

    FPrimaryAssetId AssetId(TEXT("DialogueGraph"), DialogueTag.GetTagName());
    TArray<FName> BundlesToLoad;
    FStreamableDelegate OnLoaded = FStreamableDelegate::CreateLambda(
        [this, DialogueTag, InInstigatorCharacter, InNPCActor]()
        {
            if (!UAssetManager::IsInitialized()) return;
            FPrimaryAssetId Id(TEXT("DialogueGraph"), DialogueTag.GetTagName());
            UObject* Asset = UAssetManager::Get().GetPrimaryAssetObject(Id);
            if (UDialogueGraphDataAsset* Graph = Cast<UDialogueGraphDataAsset>(Asset))
            {
                UE_LOG(LogTemp, Log, TEXT("[DialogueManager] 对话图加载完成，启动剧情: %s"), *DialogueTag.ToString());
                StartDialogue(Graph, InInstigatorCharacter, InNPCActor);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[DialogueManager] 对话图加载失败或类型错误: %s"), *DialogueTag.ToString());
            }
        }
    );

    UE_LOG(LogTemp, Log, TEXT("[DialogueManager] 异步加载对话图: %s"), *DialogueTag.ToString());
    UAssetManager::Get().LoadPrimaryAsset(AssetId, BundlesToLoad, OnLoaded);
}

void UDialogueManagerSubsystem::EndDialogue()
{
    if (!CurrentGraph) return;

    CurrentGraph = nullptr;
    CurrentNode = nullptr;
    InstigatorCharacter = nullptr;
    NPCActor = nullptr;
    CachedSpeakers.Empty();

    OnDialogueEnded.Broadcast();
}

// ============================================================================
// 对话推进
// ============================================================================

void UDialogueManagerSubsystem::AdvanceToNext()
{
    if (!CurrentNode || !CurrentGraph) return;

    // 执行当前节点的 PostActions
    ExecuteActions(CurrentNode->PostActions);

    // 跳转到下一节点
    AdvanceToNode(CurrentNode->NextNodeID);
}

void UDialogueManagerSubsystem::SelectChoice(int32 ChoiceIndex)
{
    if (!CurrentNode || CurrentNode->NodeType != EDialogueNodeType::Choice) return;
    if (ChoiceIndex < 0 || ChoiceIndex >= CurrentNode->Choices.Num()) return;

    const FDialogueChoice& Choice = CurrentNode->Choices[ChoiceIndex];

    // 执行选项动作
    ExecuteActions(Choice.Actions);

    // 跳转到选项目标节点
    AdvanceToNode(Choice.TargetNodeID);
}

// ============================================================================
// 状态查询
// ============================================================================

USpeakerDefinitionDataAsset* UDialogueManagerSubsystem::GetSpeakerDefinition(const FGameplayTag& SpeakerTag) const
{
    if (const TObjectPtr<USpeakerDefinitionDataAsset>* Found = CachedSpeakers.Find(SpeakerTag))
    {
        return Found->Get();
    }
    return nullptr;
}

// ============================================================================
// 内部
// ============================================================================

void UDialogueManagerSubsystem::ExecuteActions(const TArray<FDialogueAction>& Actions)
{
    UQuestManagerSubsystem* QuestSubsystem = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UQuestManagerSubsystem>() : nullptr;

    for (const FDialogueAction& Action : Actions)
    {
        switch (Action.Type)
        {
        case EDialogueActionType::GiveQuest:
            if (QuestSubsystem)
            {
                QuestSubsystem->AcceptQuest(Action.QuestTag);
            }
            break;

        case EDialogueActionType::CompleteQuest:
            if (QuestSubsystem)
            {
                QuestSubsystem->TurnInQuest(Action.QuestTag);
            }
            break;

        case EDialogueActionType::AdvanceObjective:
            if (QuestSubsystem)
            {
                QuestSubsystem->AdvanceObjective(Action.QuestTag, Action.ObjectiveTag);
            }
            break;

        case EDialogueActionType::PlayAnimation:
            // TODO: 播放蒙太奇（需 MotionWarping 或 SimplePlayMontage）
            break;

        case EDialogueActionType::GiveItem:
            // TODO: 通过 InventoryManagerSubsystem 给予物品
            break;

        case EDialogueActionType::StartCombat:
            // TODO: 触发战斗
            break;

        case EDialogueActionType::OpenShop:
            // TODO: 打开商店 UI
            break;

        case EDialogueActionType::None:
            break;
        }
    }
}

void UDialogueManagerSubsystem::AdvanceToNode(int32 NodeID)
{
    if (!CurrentGraph) return;

    // NodeID = -1 表示结束对话
    if (NodeID < 0)
    {
        EndDialogue();
        return;
    }

    const FDialogueNode* NextNode = CurrentGraph->FindNode(NodeID);
    if (!NextNode)
    {
        EndDialogue();
        return;
    }

    CurrentNode = NextNode;

    // 执行 PreActions
    ExecuteActions(NextNode->PreActions);

    // 按节点类型处理
    switch (NextNode->NodeType)
    {
    case EDialogueNodeType::Condition:
        EvaluateConditionNode(*NextNode);
        break;

    case EDialogueNodeType::Action:
        // Action 节点：执行 PostActions 后自动跳转
        ExecuteActions(NextNode->PostActions);
        AdvanceToNode(NextNode->NextNodeID);
        break;

    case EDialogueNodeType::End:
        // End 节点：执行 PostActions 后结束
        ExecuteActions(NextNode->PostActions);
        EndDialogue();
        break;

    case EDialogueNodeType::Speech:
    case EDialogueNodeType::Choice:
    default:
        // Speech/Choice 节点：广播给 UI 显示
        OnDialogueNodeChanged.Broadcast(*NextNode);
        break;
    }
}

void UDialogueManagerSubsystem::EvaluateConditionNode(const FDialogueNode& Node)
{
    ACharacter* PlayerChar = InstigatorCharacter.Get();
    if (!PlayerChar) return;

    // 检查玩家 ASC 是否持有条件 Tag
    bool bConditionMet = false;
    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerChar))
    {
        bConditionMet = ASC->HasMatchingGameplayTag(Node.ConditionTag);
    }

    AdvanceToNode(bConditionMet ? Node.NextNodeID : Node.FallbackNodeID);
}

void UDialogueManagerSubsystem::LoadSpeakerDefinitions(const TArray<FGameplayTag>& SpeakerTags)
{
    CachedSpeakers.Empty();

    if (!UAssetManager::IsInitialized()) return;

    for (const FGameplayTag& Tag : SpeakerTags)
    {
        FPrimaryAssetId AssetId(TEXT("SpeakerDefinition"), Tag.GetTagName());
        if (UObject* Asset = UAssetManager::Get().GetPrimaryAssetObject(AssetId))
        {
            if (USpeakerDefinitionDataAsset* Speaker = Cast<USpeakerDefinitionDataAsset>(Asset))
            {
                CachedSpeakers.Add(Tag, Speaker);
            }
        }
    }
}
