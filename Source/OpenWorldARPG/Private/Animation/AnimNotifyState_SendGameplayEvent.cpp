// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/AnimNotifyState_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_SendGameplayEvent::UAnimNotifyState_SendGameplayEvent()
{
#if WITH_EDITORONLY_DATA
	// 在动画编辑器中用不同颜色区分 State 类 Notify
	NotifyColor = FColor(54, 142, 196);
#endif
}

void UAnimNotifyState_SendGameplayEvent::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !BeginEventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 发送 Begin 事件
	// 例如：打开连招输入窗口、激活伤害判定区域
	FGameplayEventData Payload;
	Payload.EventTag = BeginEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, BeginEventTag, Payload);
}

void UAnimNotifyState_SendGameplayEvent::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !EndEventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 发送 End 事件
	// 关键保证：即使动画被强制打断（受击、眩晕、死亡等），引擎也必定调用此函数。
	// 这确保了状态逻辑闭环：有 Begin 就一定有 End。
	// 例如：关闭连招输入窗口、关闭伤害判定区域
	FGameplayEventData Payload;
	Payload.EventTag = EndEventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EndEventTag, Payload);
}

FString UAnimNotifyState_SendGameplayEvent::GetNotifyName_Implementation() const
{
	// 在动画编辑器轨道上显示 Begin→End 的 Tag 名
	// 动画师可以一眼看出窗口的打开和关闭事件
	FString BeginName = BeginEventTag.IsValid() ? BeginEventTag.GetTagName().ToString() : TEXT("?");
	FString EndName = EndEventTag.IsValid() ? EndEventTag.GetTagName().ToString() : TEXT("?");
	return FString::Printf(TEXT("Event: %s → %s"), *BeginName, *EndName);
}
