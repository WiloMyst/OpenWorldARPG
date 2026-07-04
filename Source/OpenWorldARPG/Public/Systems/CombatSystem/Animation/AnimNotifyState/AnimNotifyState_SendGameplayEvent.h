// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_SendGameplayEvent.generated.h"

/**
 * 状态/窗口事件触发 AnimNotifyState。
 *
 * 在动画区间开始时发送 BeginEventTag，结束时发送 EndEventTag。
 * 核心价值：利用 UE 底层对 AnimNotifyState 的保护机制——即使动画被强制打断
 * （如受击硬直、眩晕、死亡等），NotifyEnd 也必定会被引擎调用。
 *
 * 为什么连招/伤害判定窗口必须用 AnimNotifyState 而不是两个 AnimNotify？
 * - AnimNotify 是单帧触发，如果用 Open + Close 两个 Notify 模拟窗口，
 *   当动画中途被打断时，Close Notify 永远不会触发，导致：
 *   - 连招窗口永远打开 → 玩家可以在任何时刻触发连招，破坏战斗节奏
 *   - 伤害判定区域永远激活 → 碰撞体残留，性能和逻辑双重问题
 *   - 状态 Tag 永远存在 → 其他系统误判角色状态，引发级联 Bug
 * - AnimNotifyState 的 NotifyEnd 由引擎保证执行（包括打断场景），
 *   确保状态逻辑必定闭环：有 Begin 就一定有 End。
 *
 * 使用方式：
 * 1. 在动画编辑器中添加此 NotifyState（拖拽一段区间）
 * 2. 在 Details 面板设置 BeginEventTag 和 EndEventTag
 * 3. GA 通过 WaitGameplayEvent 监听对应 Tag
 */
UCLASS(meta = (DisplayName = "Send Gameplay Event State"))
class OPENWORLDARPG_API UAnimNotifyState_SendGameplayEvent : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_SendGameplayEvent();

	/** 区间开始时发送的事件 Tag（如：Event.Combo.WindowOpen） */
	UPROPERTY(EditAnywhere, Category = "GameplayEvent", meta = (Categories = "Event"))
	FGameplayTag BeginEventTag;

	/** 区间结束时发送的事件 Tag（如：Event.Combo.WindowClose）
	 *  引擎保证即使动画被打断，此事件也必定触发 */
	UPROPERTY(EditAnywhere, Category = "GameplayEvent", meta = (Categories = "Event"))
	FGameplayTag EndEventTag;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
