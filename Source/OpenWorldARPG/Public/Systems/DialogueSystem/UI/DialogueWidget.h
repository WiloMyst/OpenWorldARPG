// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "Systems/DialogueSystem/Data/DialogueDataTypes.h"
#include "DialogueWidget.generated.h"

class UDialogueManagerSubsystem;
class USpeakerDefinitionDataAsset;

/**
 * 对话框 Widget 基类（C++ 驱动，蓝图配置视觉）。
 *
 * 【职责】
 * - 绑定 DialogueManagerSubsystem 委托，接收对话节点变更
 * - C++ 侧用 FTimerHandle 驱动打字机效果，逐字推进并通过 BlueprintImplementableEvent 通知蓝图更新文本
 * - 提供继续/选项点击的 C++ 接口，转发给 DialogueManagerSubsystem
 *
 * 蓝图子类负责具体视觉表现（头像、文本框、选项按钮等）。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UDialogueWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UDialogueWidget(const FObjectInitializer& ObjectInitializer);

    // --- 生命周期 ---

    /** 绑定到 DialogueManagerSubsystem */
    virtual void NativeOnInitialized() override;

    // --- C++ 接口（蓝图调用）---

    /** 玩家点击"继续"（Speech 节点推进）。打字机未完成时快进显示全文，否则推进下一节点。 */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void HandleContinueClicked();

    /** 玩家选择选项（Choice 节点） */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void HandleChoiceSelected(int32 ChoiceIndex);

    // --- 蓝图实现事件 ---

    /** 当前说话人变更 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnSpeakerChanged(USpeakerDefinitionDataAsset* Speaker, EDialogueEmotion Emotion);

    /** 打字机推进一个字符 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnTypewriterTick(const FText& CurrentVisibleText);

    /** 对话文本全部显示完毕 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnTypewriterComplete();

    /** 选项列表变更 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnChoicesChanged(const TArray<FDialogueChoice>& Choices);

    /** 对话开始 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnDialogueStarted();

    /** 对话结束 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void OnDialogueEnded();

protected:
    // --- DialogueManagerSubsystem 委托回调 ---

    UFUNCTION()
    void HandleDialogueNodeChanged(const FDialogueNode& Node);

    UFUNCTION()
    void HandleDialogueStarted(FGameplayTag DialogueTag, const TArray<FGameplayTag>& SpeakerTags);

    UFUNCTION()
    void HandleDialogueEnded();

    // --- 打字机 ---

    /** 启动打字机效果 */
    void StartTypewriter(const FText& FullText);

    /** 打字机逐字推进（Timer 回调） */
    UFUNCTION()
    void TypewriterTick();

    /** 停止打字机 */
    void StopTypewriter();

    // --- 缓存 ---

    UPROPERTY()
    TObjectPtr<UDialogueManagerSubsystem> DialogueSubsystem;

    // --- 打字机状态 ---

    /** 当前完整文本 */
    FText CurrentFullText;

    /** 当前完整字符串（用于逐字切片） */
    FString CurrentFullString;

    /** 当前已显示到的字符索引 */
    int32 CurrentCharIndex = 0;

    /** 打字机定时器句柄 */
    FTimerHandle TypewriterTimerHandle;

    /** 打字机是否已完成 */
    bool bTypewriterComplete = false;

    // --- 打字机配置 ---

    /** 单字间隔（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Typewriter")
    float CharInterval = 0.03f;

    /** 逗号处暂停时长（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Typewriter")
    float CommaPauseDuration = 0.15f;

    /** 句号处暂停时长（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Dialogue|Typewriter")
    float PeriodPauseDuration = 0.3f;
};
