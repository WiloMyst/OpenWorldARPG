// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/QuestSystem/UI/QuestLogWidget.h"
#include "Systems/QuestSystem/QuestManagerSubsystem.h"

UQuestLogWidget::UQuestLogWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UQuestLogWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            QuestSubsystem = LocalPlayer->GetSubsystem<UQuestManagerSubsystem>();
            if (QuestSubsystem)
            {
                QuestSubsystem->OnQuestStateChanged.AddDynamic(this, &UQuestLogWidget::HandleQuestStateChanged);
            }
        }
    }

    // 初始刷新一次，确保打开时即显示当前任务状态
    RefreshQuestList();
}

void UQuestLogWidget::HandleQuestStateChanged(const FQuestStateChangedPayload& Payload)
{
    RefreshQuestList();
}

void UQuestLogWidget::RefreshQuestList()
{
    if (!QuestSubsystem)
    {
        return;
    }

    TArray<FGameplayTag> ActiveQuestTags;
    TArray<FGameplayTag> CompletedQuestTags;

    QuestSubsystem->GetQuestsByState(EQuestState::Active, ActiveQuestTags);
    QuestSubsystem->GetQuestsByState(EQuestState::Completed, CompletedQuestTags);

    OnQuestListRefreshed(ActiveQuestTags, CompletedQuestTags);
}

void UQuestLogWidget::SetTrackedQuest(const FGameplayTag& QuestTag)
{
    if (QuestSubsystem)
    {
        QuestSubsystem->SetTrackedQuest(QuestTag);
    }
}
