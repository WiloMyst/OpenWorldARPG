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

    // 完美解耦：直接从 ActorInfo 获取 ASC，不需要进行任何强转！
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 对应图1：Apply Gameplay Effect To Self
    if (ClimbingStateEffectClass)
    {
        FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ClimbingStateEffectClass, 1.0f, EffectContext);
        if (SpecHandle.IsValid())
        {
            // 应用 GE 并将句柄保存起来
            ActiveClimbEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 2. 对应图2：Wait Gameplay Event
    if (StopClimbEventTag.IsValid())
    {
        // 这里的参数 true 代表 OnlyMatchExact，完美对应你蓝图里的勾选
        WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopClimbEventTag, nullptr, false, true);
        if (WaitEventTask)
        {
            // 绑定回调，并激活任务
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
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // 1. 对应图1与图2：OnEndAbility -> Remove Active Gameplay Effect
    if (ASC && ActiveClimbEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(ActiveClimbEffectHandle);
        ActiveClimbEffectHandle.Invalidate(); // 清空句柄
    }

    // 2. 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
    }

    // 最后必须调用 Super
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}