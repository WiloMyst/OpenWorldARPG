// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/DialogueSystem/UI/DialogueWidget.h"
#include "Systems/DialogueSystem/DialogueManagerSubsystem.h"
#include "Systems/DialogueSystem/Data/SpeakerDefinitionDataAsset.h"
#include "TimerManager.h"
#include "Engine/World.h"

UDialogueWidget::UDialogueWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UDialogueWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            DialogueSubsystem = LocalPlayer->GetSubsystem<UDialogueManagerSubsystem>();
            if (DialogueSubsystem)
            {
                DialogueSubsystem->OnDialogueStarted.AddDynamic(this, &UDialogueWidget::HandleDialogueStarted);
                DialogueSubsystem->OnDialogueEnded.AddDynamic(this, &UDialogueWidget::HandleDialogueEnded);
                DialogueSubsystem->OnDialogueNodeChanged.AddDynamic(this, &UDialogueWidget::HandleDialogueNodeChanged);
            }
        }
    }
}

void UDialogueWidget::HandleContinueClicked()
{
    if (!bTypewriterComplete)
    {
        // 打字机未完成：快进，直接显示全文并标记完成
        StopTypewriter();
        bTypewriterComplete = true;
        OnTypewriterTick(CurrentFullText);
        OnTypewriterComplete();
        return;
    }

    // 打字机已完成：推进到下一节点
    if (DialogueSubsystem)
    {
        DialogueSubsystem->AdvanceToNext();
    }
}

void UDialogueWidget::HandleChoiceSelected(int32 ChoiceIndex)
{
    if (DialogueSubsystem)
    {
        DialogueSubsystem->SelectChoice(ChoiceIndex);
    }
}

void UDialogueWidget::HandleDialogueNodeChanged(const FDialogueNode& Node)
{
    StopTypewriter();

    switch (Node.NodeType)
    {
    case EDialogueNodeType::Speech:
    {
        // Speech 节点：更新说话人 + 启动打字机
        USpeakerDefinitionDataAsset* Speaker = DialogueSubsystem ? DialogueSubsystem->GetSpeakerDefinition(Node.SpeakerTag) : nullptr;
        OnSpeakerChanged(Speaker, Node.Emotion);
        StartTypewriter(Node.SpeechText);
        break;
    }
    case EDialogueNodeType::Choice:
    {
        // Choice 节点：显示问题文本（打字机）+ 选项列表
        StartTypewriter(Node.SpeechText);
        OnChoicesChanged(Node.Choices);
        break;
    }
    default:
        // End/Action/Condition 节点：由 Subsystem 自动推进，UI 无需更新
        break;
    }
}

void UDialogueWidget::HandleDialogueStarted(FGameplayTag DialogueTag, const TArray<FGameplayTag>& SpeakerTags)
{
    OnDialogueStarted();
}

void UDialogueWidget::HandleDialogueEnded()
{
    StopTypewriter();
    OnDialogueEnded();
}

void UDialogueWidget::StartTypewriter(const FText& FullText)
{
    StopTypewriter();

    CurrentFullText = FullText;
    CurrentFullString = FullText.ToString();
    CurrentCharIndex = 0;
    bTypewriterComplete = false;

    // 空文本直接完成
    if (CurrentFullString.IsEmpty())
    {
        bTypewriterComplete = true;
        OnTypewriterComplete();
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TypewriterTimerHandle, this, &UDialogueWidget::TypewriterTick, CharInterval, false);
    }
}

void UDialogueWidget::TypewriterTick()
{
    if (CurrentCharIndex >= CurrentFullString.Len())
    {
        // 已到达末尾
        StopTypewriter();
        bTypewriterComplete = true;
        OnTypewriterComplete();
        return;
    }

    // 取当前字符并推进索引
    const TCHAR CurrentChar = CurrentFullString[CurrentCharIndex];
    CurrentCharIndex++;

    // 构造当前可见子字符串，通知蓝图更新文本
    const FString VisibleString = CurrentFullString.Left(CurrentCharIndex);
    OnTypewriterTick(FText::FromString(VisibleString));

    // 根据刚显示的字符计算下一次 Tick 的延迟（标点处暂停）
    float NextDelay = CharInterval;
    if (CurrentChar == TEXT(',') || CurrentChar == TEXT('，'))
    {
        NextDelay = CommaPauseDuration;
    }
    else if (CurrentChar == TEXT('.') || CurrentChar == TEXT('。') ||
             CurrentChar == TEXT('!') || CurrentChar == TEXT('！') ||
             CurrentChar == TEXT('?') || CurrentChar == TEXT('？'))
    {
        NextDelay = PeriodPauseDuration;
    }

    // 重新调度（非循环，按字符延迟推进）
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TypewriterTimerHandle, this, &UDialogueWidget::TypewriterTick, NextDelay, false);
    }
}

void UDialogueWidget::StopTypewriter()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TypewriterTimerHandle);
    }
}
