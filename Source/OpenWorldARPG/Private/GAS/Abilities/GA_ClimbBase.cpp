// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_ClimbBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UGA_ClimbBase::UGA_ClimbBase()
{
    // 对应图3：设置高级默认项
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_ClimbBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // GAS 规范：即使没有逻辑，也必须尝试 Commit，成功后才能继续
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 状态 Tag (Character.State.Climbing) 由 CMC 的 OnMovementModeChanged 统一管理，
    // GA 不再通过 GE 重复注入，避免 Tag 计数冲突

    // 监听停止事件 (意愿层：等待玩家输入或系统取消)
    if (StopClimbEventTag.IsValid())
    {
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopClimbEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            WaitEventTask->EventReceived.AddDynamic(this, &UGA_ClimbBase::OnStopClimbEventReceived);
            WaitEventTask->ReadyForActivation();
        }
    }
}

void UGA_ClimbBase::OnStopClimbEventReceived(FGameplayEventData Payload)
{
    // 收到停止攀爬的 Tag 广播，优雅结束技能
    bool bReplicateEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ClimbBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 状态 Tag 由 CMC 统一管理，GA 不再负责 GE 的移除

    // 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}