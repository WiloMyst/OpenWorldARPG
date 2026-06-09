// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_DieBase.h"
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

    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

    // 只要这个 Actor “实现了”战斗接口，不管他是主角、哥布林还是木箱子，直接让他执行死亡
    if (ActorInfo->AvatarActor.IsValid() && ActorInfo->AvatarActor->Implements<UCombatInterface>())
    {
        // 虚幻接口调用的标准宏：Execute_XXX
        ICombatInterface::Execute_HandleDeath(ActorInfo->AvatarActor.Get());
    }
}