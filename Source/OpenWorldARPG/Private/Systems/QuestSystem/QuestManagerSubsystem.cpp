// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/QuestSystem/QuestManagerSubsystem.h"
#include "Systems/QuestSystem/Data/QuestDefinitionDataAsset.h"
#include "Engine/AssetManager.h"

void UQuestManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

// ============================================================================
// 状态查询
// ============================================================================

EQuestState UQuestManagerSubsystem::GetQuestState(const FGameplayTag& QuestTag) const
{
    if (const FQuestProgress* Progress = QuestProgressMap.Find(QuestTag))
    {
        return Progress->State;
    }
    return EQuestState::Locked;
}

bool UQuestManagerSubsystem::IsQuestCompleted(const FGameplayTag& QuestTag) const
{
    return GetQuestState(QuestTag) == EQuestState::Completed;
}

bool UQuestManagerSubsystem::IsQuestInState(const FGameplayTag& QuestTag, EQuestState State) const
{
    return GetQuestState(QuestTag) == State;
}

void UQuestManagerSubsystem::GetQuestsByState(EQuestState State, TArray<FGameplayTag>& OutQuestTags) const
{
    for (const auto& Pair : QuestProgressMap)
    {
        if (Pair.Value.State == State)
        {
            OutQuestTags.Add(Pair.Key);
        }
    }
}

// ============================================================================
// 任务操作
// ============================================================================

bool UQuestManagerSubsystem::AcceptQuest(const FGameplayTag& QuestTag)
{
    EQuestState CurrentState = GetQuestState(QuestTag);

    // 仅 Locked 或 Available 状态可接取
    if (CurrentState != EQuestState::Locked && CurrentState != EQuestState::Available)
    {
        return false;
    }

    // 检查前置条件
    if (!ArePrerequisitesMet(QuestTag))
    {
        return false;
    }

    // 初始化进度
    FQuestProgress& Progress = QuestProgressMap.Add(QuestTag);
    Progress.State = EQuestState::Active;
    Progress.AcceptTime = FDateTime::UtcNow();
    Progress.bTracked = (TrackedQuestTag.IsValid() == false);

    if (Progress.bTracked)
    {
        TrackedQuestTag = QuestTag;
    }

    SetQuestState(QuestTag, EQuestState::Active);
    return true;
}

int32 UQuestManagerSubsystem::AdvanceObjective(const FGameplayTag& QuestTag, const FGameplayTag& ObjectiveTag, int32 DeltaCount)
{
    FQuestProgress* Progress = QuestProgressMap.Find(QuestTag);
    if (!Progress || Progress->State != EQuestState::Active)
    {
        return -1;
    }

    int32& CurrentCount = Progress->ObjectiveCounts.FindOrAdd(ObjectiveTag);
    CurrentCount = FMath::Max(0, CurrentCount + DeltaCount);

    OnQuestObjectiveProgress.Broadcast(QuestTag, ObjectiveTag, CurrentCount);

    // 检查是否所有目标完成
    if (AreAllObjectivesCompleted(QuestTag))
    {
        SetQuestState(QuestTag, EQuestState::ReadyToTurnIn);
    }

    return CurrentCount;
}

bool UQuestManagerSubsystem::TurnInQuest(const FGameplayTag& QuestTag)
{
    if (GetQuestState(QuestTag) != EQuestState::ReadyToTurnIn)
    {
        return false;
    }

    SetQuestState(QuestTag, EQuestState::Completed);

    // 清除追踪
    if (TrackedQuestTag == QuestTag)
    {
        TrackedQuestTag = FGameplayTag::EmptyTag;
    }

    return true;
}

void UQuestManagerSubsystem::FailQuest(const FGameplayTag& QuestTag)
{
    if (GetQuestState(QuestTag) == EQuestState::Active)
    {
        SetQuestState(QuestTag, EQuestState::Failed);
    }
}

void UQuestManagerSubsystem::SetTrackedQuest(const FGameplayTag& QuestTag)
{
    if (QuestProgressMap.Contains(QuestTag))
    {
        // 清除旧追踪
        if (TrackedQuestTag.IsValid())
        {
            if (FQuestProgress* OldProgress = QuestProgressMap.Find(TrackedQuestTag))
            {
                OldProgress->bTracked = false;
            }
        }

        TrackedQuestTag = QuestTag;

        if (FQuestProgress* NewProgress = QuestProgressMap.Find(QuestTag))
        {
            NewProgress->bTracked = true;
        }
    }
}

// ============================================================================
// 存档接口
// ============================================================================

void UQuestManagerSubsystem::LoadFromSaveData(const TMap<FGameplayTag, FQuestProgress>& InQuestProgressMap)
{
    QuestProgressMap = InQuestProgressMap;

    // 恢复追踪任务
    for (const auto& Pair : QuestProgressMap)
    {
        if (Pair.Value.bTracked)
        {
            TrackedQuestTag = Pair.Key;
            break;
        }
    }
}

void UQuestManagerSubsystem::CollectSaveData(TMap<FGameplayTag, FQuestProgress>& OutQuestProgressMap) const
{
    OutQuestProgressMap = QuestProgressMap;
}

// ============================================================================
// 任务定义加载
// ============================================================================

void UQuestManagerSubsystem::LoadQuestDefinition(const FGameplayTag& QuestTag, TFunction<void(UQuestDefinitionDataAsset*)> Callback)
{
    // 检查缓存
    if (UQuestDefinitionDataAsset* Cached = GetCachedDefinition(QuestTag))
    {
        Callback(Cached);
        return;
    }

    // 异步加载
    FPrimaryAssetId AssetId(TEXT("QuestDefinition"), QuestTag.GetTagName());
    UAssetManager::Get().LoadPrimaryAsset(AssetId, {}, FStreamableDelegate::CreateLambda(
        [this, QuestTag, Callback]()
        {
            UQuestDefinitionDataAsset* Definition = GetCachedDefinition(QuestTag);
            Callback(Definition);
        }));
}

// ============================================================================
// 内部
// ============================================================================

bool UQuestManagerSubsystem::ArePrerequisitesMet(const FGameplayTag& QuestTag) const
{
    UQuestDefinitionDataAsset* Definition = GetCachedDefinition(QuestTag);
    if (!Definition) return true; // 无定义时放行（容错）

    // 检查前置任务
    for (const FGameplayTag& PrereqTag : Definition->PrerequisiteQuestTags)
    {
        if (!IsQuestCompleted(PrereqTag))
        {
            return false;
        }
    }

    return true;
}

bool UQuestManagerSubsystem::AreAllObjectivesCompleted(const FGameplayTag& QuestTag) const
{
    UQuestDefinitionDataAsset* Definition = GetCachedDefinition(QuestTag);
    if (!Definition) return false;

    const FQuestProgress* Progress = QuestProgressMap.Find(QuestTag);
    if (!Progress) return false;

    for (const FQuestObjective& Objective : Definition->Objectives)
    {
        if (Objective.bOptional) continue;

        const int32* CurrentCount = Progress->ObjectiveCounts.Find(Objective.ObjectiveTag);
        if (!CurrentCount || *CurrentCount < Objective.RequiredCount)
        {
            return false;
        }
    }

    return true;
}

void UQuestManagerSubsystem::SetQuestState(const FGameplayTag& QuestTag, EQuestState NewState)
{
    FQuestProgress* Progress = QuestProgressMap.Find(QuestTag);
    if (!Progress) return;

    EQuestState OldState = Progress->State;
    if (OldState == NewState) return;

    Progress->State = NewState;

    FQuestStateChangedPayload Payload;
    Payload.QuestTag = QuestTag;
    Payload.OldState = OldState;
    Payload.NewState = NewState;
    OnQuestStateChanged.Broadcast(Payload);
}

UQuestDefinitionDataAsset* UQuestManagerSubsystem::GetCachedDefinition(const FGameplayTag& QuestTag) const
{
    if (const TObjectPtr<UQuestDefinitionDataAsset>* Found = CachedDefinitions.Find(QuestTag))
    {
        return Found->Get();
    }

    // 尝试从 AssetManager 获取已加载的资产
    FPrimaryAssetId AssetId(TEXT("QuestDefinition"), QuestTag.GetTagName());
    if (UAssetManager::IsInitialized())
    {
        return Cast<UQuestDefinitionDataAsset>(UAssetManager::Get().GetPrimaryAssetObject(AssetId));
    }

    return nullptr;
}
