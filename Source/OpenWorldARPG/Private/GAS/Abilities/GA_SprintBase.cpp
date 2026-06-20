// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_SprintBase.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
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
    // O(1) 读取缓存的 CMC 指针，替代 FindComponentByClass O(N) 遍历
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!CustomMoveComp || !Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 读取 TriggerEventData 中的 EventMagnitude 判断短/长疾跑
    // EventMagnitude: 1.0 = 长按（持久疾跑），0.0 = 点按（短时疾跑）
    const float EventMagnitude = TriggerEventData ? TriggerEventData->EventMagnitude : 1.0f;
    bIsShortSprint = (EventMagnitude < 0.5f);

    // CMC 职责：修改 MaxWalkSpeed
    CustomMoveComp->EnterSprintMode();

    // 鸣潮规则：Sprint 全程不消耗体力，体力已在 Dash 激活时一次性扣除

    // 短疾跑：使用 AbilityTask_WaitDelay 而非原生 Timer
    // AbilityTask 自动绑定网络预测键（PredictionKey），确保客户端和服务器双端完美同步
    if (bIsShortSprint && ShortSprintDuration > 0.0f)
    {
        ShortSprintDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ShortSprintDuration);
        if (ShortSprintDelayTask)
        {
            ShortSprintDelayTask->OnFinish.AddDynamic(this, &UGA_SprintBase::OnShortSprintExpired);
            ShortSprintDelayTask->ReadyForActivation();
        }
    }

    // 启动定时检测：移动输入归零（长疾跑的唯一结束条件）
    if (ConditionCheckInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(ConditionCheckTimer, this, &UGA_SprintBase::CheckSprintConditions, ConditionCheckInterval, true);
    }
}

void UGA_SprintBase::OnShortSprintExpired()
{
    // 短疾跑定时器到期，正常结束疾跑
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SprintBase::CheckSprintConditions()
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character) return;

    // 检测：移动输入是否归零（鸣潮规则：松开方向键立即结束疾跑）
    UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
    if (MoveComp && MoveComp->GetLastInputVector().SquaredLength() <= MovementInputThreshold)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 鸣潮规则：Sprint 全程不消耗体力，无体力耗尽检测
    // 鸣潮规则：进入疾跑后，松开冲刺键不会打断疾跑，疾跑仅由方向键管理
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

    // 停止状态检测定时器
    GetWorld()->GetTimerManager().ClearTimer(ConditionCheckTimer);

    // 停止短疾跑 AbilityTask（自动清理预测键绑定）
    if (ShortSprintDelayTask)
    {
        ShortSprintDelayTask->EndTask();
        ShortSprintDelayTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
