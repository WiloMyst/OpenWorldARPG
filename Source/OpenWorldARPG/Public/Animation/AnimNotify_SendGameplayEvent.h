// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_SendGameplayEvent.generated.h"

/**
 * 单点事件触发 AnimNotify。
 *
 * 在动画指定帧触发一次 GAS Gameplay Event。
 * 用途：攻击命中判定、特效触发、音效触发、连招输入开放等单帧事件。
 *
 * 使用方式：
 * 1. 在动画编辑器中添加此 Notify
 * 2. 在 Details 面板的 EventTag 下拉菜单中选择要发送的 Gameplay Tag
 * 3. 对应的 GA 通过 WaitGameplayEvent 监听该 Tag 即可
 *
 * 相比蓝图硬编码 SendGameplayEventToActor 的优势：
 * - 数据驱动：Tag 在编辑器中选择，无需蓝图节点，无硬编码
 * - 可维护性：所有事件 Tag 在动画编辑器轨道上直接可见
 * - 性能：省去蓝图虚拟机开销
 */
UCLASS(const, hidecategories = Object, meta = (DisplayName = "Send Gameplay Event"))
class OPENWORLDARPG_API UAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_SendGameplayEvent();

	/** 要发送的 Gameplay Event Tag */
	UPROPERTY(EditAnywhere, Category = "GameplayEvent", meta = (Categories = "Event"))
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
