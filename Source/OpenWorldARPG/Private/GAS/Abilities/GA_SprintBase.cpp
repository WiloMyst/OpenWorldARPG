// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_SprintBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_SprintBase::UGA_SprintBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_SprintBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    if (!ASC || !CustomMoveComp || !Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // CMC 职责：修改 MaxWalkSpeed
    // GA 只发送"进入极速跑"的意愿，不传递任何物理参数
    CustomMoveComp->EnterSprintMode();

    // GA 职责：意愿和表现

    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

    // 应用持续扣减体力的 GameplayEffect
    if (SprintStaminaDrainGE && ASC)
    {
        UGameplayEffect* GECDO = SprintStaminaDrainGE->GetDefaultObject<UGameplayEffect>();
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SprintStaminaDrainGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            StaminaDrainGEHandle = ActiveHandle;
        }
    }

    // 监听停止事件（意愿层：等待玩家释放 Shift 或系统取消）
    if (StopSprintEventTag.IsValid())
    {
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopSprintEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            WaitEventTask->EventReceived.AddDynamic(this, &UGA_SprintBase::OnStopSprintEventReceived);
            WaitEventTask->ReadyForActivation();
        }
    }

    // 启动定时检测：移动输入归零、体力耗尽、Shift 释放
    if (ConditionCheckInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(ConditionCheckTimer, this, &UGA_SprintBase::CheckSprintConditions, ConditionCheckInterval, true);
    }
}

void UGA_SprintBase::OnStopSprintEventReceived(FGameplayEventData Payload)
{
    // 收到 SprintStop 事件（Shift 释放），结束极速跑
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SprintBase::CheckSprintConditions()
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character) return;

    // 检测1：移动输入是否归零
    UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
    if (MoveComp && MoveComp->GetLastInputVector().SquaredLength() <= MovementInputThreshold)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 检测2：体力是否耗尽
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        const float CurrentStamina = ASC->GetNumericAttribute(UAS_Player::GetStaminaAttribute());
        if (CurrentStamina <= MinStaminaToSprint)
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
            return;
        }
    }

    // 检测3：Shift 是否仍然按住（防御性检测，防止 StopSprintEvent 丢失）
    APlayerController* PC = Character->GetController<APlayerController>();
    if (PC && !PC->IsInputKeyDown(EKeys::LeftShift))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }
}

void UGA_SprintBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // O(1) 读取缓存的 CMC 指针
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    // CMC 职责：恢复 MaxWalkSpeed
    if (CustomMoveComp && CustomMoveComp->IsSprinting())
    {
        CustomMoveComp->ExitSprintMode();
    }

    // GA 职责：清理意愿和表现

    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 移除持续扣减体力的 GameplayEffect
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (ASC && StaminaDrainGEHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(StaminaDrainGEHandle);
        StaminaDrainGEHandle = FActiveGameplayEffectHandle();
    }

    // 停止状态检测定时器
    GetWorld()->GetTimerManager().ClearTimer(ConditionCheckTimer);

    // 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
