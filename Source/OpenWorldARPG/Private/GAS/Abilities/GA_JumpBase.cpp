// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_JumpBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_JumpBase::UGA_JumpBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_JumpBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    UE_LOG(LogTemp, Warning, TEXT("[Jump] GA_JumpBase::ActivateAbility called"));

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogTemp, Error, TEXT("[Jump] CommitAbility failed! Cannot jump."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 通用化：直接操作 Character，不依赖特定蓝图类
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Jump] Character found, calling Jump(). IsWalking=%d, IsFalling=%d, MovementMode=%d"),
            Character->GetCharacterMovement()->IsWalking(),
            Character->GetCharacterMovement()->IsFalling(),
            (int32)Character->GetCharacterMovement()->MovementMode.GetValue());
        Character->Jump();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Jump] AvatarActor is not a Character! Cannot jump."));
    }

    // 对应蓝图：监听停止跳跃事件
    if (StopJumpEventTag.IsValid())
    {
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopJumpEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            WaitEventTask->EventReceived.AddDynamic(this, &UGA_JumpBase::OnStopJumpEventReceived);
            WaitEventTask->ReadyForActivation();
        }
    }
}

void UGA_JumpBase::OnStopJumpEventReceived(FGameplayEventData Payload)
{
    // 收到停止事件，结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_JumpBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Warning, TEXT("[Jump] GA_JumpBase::EndAbility called, bWasCancelled=%d"), bWasCancelled);

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        // 对应蓝图：停止跳跃
        Character->StopJumping();
    }

    if (WaitEventTask) WaitEventTask->EndTask();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}