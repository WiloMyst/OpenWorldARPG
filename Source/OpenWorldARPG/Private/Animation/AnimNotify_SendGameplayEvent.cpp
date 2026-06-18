// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/AnimNotify_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

UAnimNotify_SendGameplayEvent::UAnimNotify_SendGameplayEvent()
{
#if WITH_EDITORONLY_DATA
	// 在动画编辑器中显示为可识别的颜色
	NotifyColor = FColor(196, 142, 54);
#endif
}

void UAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 发送空 Payload 的 Gameplay Event
	// GA 通过 WaitGameplayEvent 或 GA Trigger 监听此 Tag 即可收到
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}

FString UAnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	// 在动画编辑器轨道上直接显示 Tag 名，而非类名
	// 动画师可以一眼看出每个 Notify 发送的是什么事件
	if (EventTag.IsValid())
	{
		return FString::Printf(TEXT("Event: %s"), *EventTag.GetTagName().ToString());
	}
	return TEXT("Send Gameplay Event (No Tag)");
}
