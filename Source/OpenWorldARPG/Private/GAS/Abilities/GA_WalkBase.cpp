// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_WalkBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
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
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;

    if (!ASC || !CustomMoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // CMC 职责：修改 MaxWalkSpeed
    // GA 只发送"进入慢走"的意愿，不传递任何物理参数
    // ==========================================
    CustomMoveComp->EnterWalkMode();

    // ==========================================
    // GA 职责：意愿和表现
    // ==========================================

    // 1. 应用状态 GE (意愿层：Tag 状态标记)
    if (WalkStateEffectClass)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(WalkStateEffectClass, 1.0f, ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            ActiveWalkEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 2. 监听停止事件 (意愿层：等待玩家输入或系统取消)
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
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;

    // ==========================================
    // CMC 职责：恢复 MaxWalkSpeed
    // ==========================================
    if (CustomMoveComp && CustomMoveComp->IsWalking())
    {
        CustomMoveComp->ExitWalkMode();
    }

    // ==========================================
    // GA 职责：清理意愿和表现
    // ==========================================

    // 1. 移除慢走 GE (意愿层)
    if (ASC && ActiveWalkEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(ActiveWalkEffectHandle);
        ActiveWalkEffectHandle.Invalidate();
    }

    // 2. 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
