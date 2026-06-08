// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_AimBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"

UGA_AimBase::UGA_AimBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_AimBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;

    if (!Character || !CustomMoveComp || !ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // CMC 职责：修改 MaxWalkSpeed、旋转模式
    // GA 只发送"进入瞄准"的意愿，不传递任何物理参数
    // ==========================================
    CustomMoveComp->EnterAimMode();

    // ==========================================
    // GA 职责：意愿和表现
    // ==========================================

    // 1. 发送蒙太奇事件到武器组件 (意愿层：通知武器组件播放瞄准动画)
    if (AimMontageEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, AimMontageEventTag, FGameplayEventData());
    }

    // 2. 应用状态 GE (意愿层：Tag 状态标记)
    if (AimStateEffectClass)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(AimStateEffectClass, 1.0f, ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            ActiveAimEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 3. 监听停止事件 (意愿层：等待玩家输入或系统取消)
    if (StopAimEventTag.IsValid())
    {
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopAimEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            WaitEventTask->EventReceived.AddDynamic(this, &UGA_AimBase::OnStopAimEventReceived);
            WaitEventTask->ReadyForActivation();
        }
    }
}

void UGA_AimBase::OnStopAimEventReceived(FGameplayEventData Payload)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AimBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;

    // ==========================================
    // CMC 职责：恢复 MaxWalkSpeed、旋转模式
    // ==========================================
    if (CustomMoveComp && CustomMoveComp->IsAiming())
    {
        CustomMoveComp->ExitAimMode();
    }

    // ==========================================
    // GA 职责：清理意愿和表现
    // ==========================================

    // 1. 移除瞄准 GE (意愿层)
    if (ASC && ActiveAimEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(ActiveAimEffectHandle);
        ActiveAimEffectHandle.Invalidate();
    }

    // 2. 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
