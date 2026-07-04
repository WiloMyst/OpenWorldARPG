// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MovementSystem/Abilities/GA_WalkBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Systems/MovementSystem/Components/PlayerCharacterMovementComponent.h"
#include "Systems/AbilitySystem/ARPGGameplayAbilityActorInfo.h"
#include "GameFramework/Character.h"

UGA_WalkBase::UGA_WalkBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_WalkBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    // O(1) 读取缓存的 CMC 指针，替代 FindComponentByClass O(N) 遍历
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UPlayerCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!ASC || !CustomMoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

        // CMC 职责：修改 MaxWalkSpeed
    // GA 只发送"进入慢走"的意愿，不传递任何物理参数
        CustomMoveComp->EnterWalkMode();

        // GA 职责：意愿和表现
    
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

    // 监听停止事件 (意愿层：等待玩家输入或系统取消)
    if (StopWalkEventTag.IsValid())
    {
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopWalkEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            WaitEventTask->EventReceived.AddDynamic(this, &UGA_WalkBase::OnStopWalkEventReceived);
            WaitEventTask->ReadyForActivation();
        }
    }
}

void UGA_WalkBase::OnStopWalkEventReceived(FGameplayEventData Payload)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_WalkBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // O(1) 读取缓存的 CMC 指针
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UPlayerCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

        // CMC 职责：恢复 MaxWalkSpeed
        if (CustomMoveComp && CustomMoveComp->IsWalking())
    {
        CustomMoveComp->ExitWalkMode();
    }

        // GA 职责：清理意愿和表现
    
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
