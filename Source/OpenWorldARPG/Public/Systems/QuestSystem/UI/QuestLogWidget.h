// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "Systems/QuestSystem/Data/QuestTypes.h"
#include "QuestLogWidget.generated.h"

class UQuestManagerSubsystem;
class UQuestDefinitionDataAsset;

/**
 * 任务日志 Widget 基类（C++ 驱动，蓝图配置视觉）。
 *
 * 【职责】
 * - 绑定 QuestManagerSubsystem 委托，接收任务状态变更
 * - 任务状态变化时刷新任务列表（通过 BlueprintImplementableEvent 通知蓝图）
 * - 提供设置追踪任务的 C++ 接口
 *
 * 蓝图子类负责具体视觉表现（任务条目、详情面板等）。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UQuestLogWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UQuestLogWidget(const FObjectInitializer& ObjectInitializer);

    /** 绑定到 QuestManagerSubsystem */
    virtual void NativeOnInitialized() override;

    // --- 蓝图实现事件 ---

    /** 刷新任务列表（活动 + 已完成） */
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest")
    void OnQuestListRefreshed(const TArray<FGameplayTag>& ActiveQuestTags, const TArray<FGameplayTag>& CompletedQuestTags);

    /** 刷新单个任务详情 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest")
    void OnQuestDetailUpdated(UQuestDefinitionDataAsset* Definition, const FQuestProgress& Progress);

    // --- C++ 接口（蓝图调用）---

    /** 设置追踪任务（屏幕指引/小地图标记） */
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void SetTrackedQuest(const FGameplayTag& QuestTag);

protected:
    // --- QuestManagerSubsystem 委托回调 ---

    UFUNCTION()
    void HandleQuestStateChanged(const FQuestStateChangedPayload& Payload);

    /** 刷新任务列表（查询子系统并通知蓝图） */
    void RefreshQuestList();

    // --- 缓存 ---

    UPROPERTY()
    TObjectPtr<UQuestManagerSubsystem> QuestSubsystem;
};
