// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_DieBase.h"
#include "AbilitySystemComponent.h"
#include "Interfaces/CombatInterface.h"

UGA_DieBase::UGA_DieBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_DieBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
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

    if (DeadStateEffectClass)
    {
        FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DeadStateEffectClass, 1.0f, EffectContext);
        if (SpecHandle.IsValid())
        {
            ActiveDeadEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 只要这个 Actor “实现了”战斗接口，不管他是主角、哥布林还是木箱子，直接让他执行死亡
    if (ActorInfo->AvatarActor.IsValid() && ActorInfo->AvatarActor->Implements<UCombatInterface>())
    {
        // 虚幻接口调用的标准宏：Execute_XXX
        ICombatInterface::Execute_HandleDeath(ActorInfo->AvatarActor.Get());
    }
}

void UGA_DieBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // 对应图1下方事件：OnEndAbility 时移除死亡状态的 GE (用于支持未来的复活机制)
    if (ASC && ActiveDeadEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(ActiveDeadEffectHandle);
        ActiveDeadEffectHandle.Invalidate();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}