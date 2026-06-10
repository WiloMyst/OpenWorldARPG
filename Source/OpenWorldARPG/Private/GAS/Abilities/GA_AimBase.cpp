// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_AimBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/WeaponManagerComponent.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
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
    // O(1) 读取缓存的 CMC 指针，替代 FindComponentByClass O(N) 遍历
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!Character || !CustomMoveComp || !ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

        // CMC 职责：修改 MaxWalkSpeed、旋转模式
    // GA 只发送"进入瞄准"的意愿，不传递任何物理参数
        CustomMoveComp->EnterAimMode();

        // GA 职责：意愿和表现

    // 0. 武器拿到手上
    if (UWeaponManagerComponent* WeaponComp = Character->FindComponentByClass<UWeaponManagerComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 1. 发送蒙太奇事件到武器组件 (意愿层：通知武器组件播放瞄准动画)
    if (AimMontageEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, AimMontageEventTag, FGameplayEventData());
    }

    // 2. 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

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
    // O(1) 读取缓存的 CMC 指针
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

        // CMC 职责：恢复 MaxWalkSpeed、旋转模式
        if (CustomMoveComp && CustomMoveComp->IsAiming())
    {
        CustomMoveComp->ExitAimMode();
    }

        // GA 职责：清理意愿和表现
    
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 被其他 GA Cancel 时，武器回到背上
    if (bWasCancelled)
    {
        ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
        if (Character)
        {
            if (UWeaponManagerComponent* WeaponComp = Character->FindComponentByClass<UWeaponManagerComponent>())
            {
                WeaponComp->WeaponToBack();
            }
        }
    }

    // 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
