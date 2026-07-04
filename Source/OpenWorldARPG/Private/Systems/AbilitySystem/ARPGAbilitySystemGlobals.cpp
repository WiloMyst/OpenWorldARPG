// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AbilitySystem/ARPGAbilitySystemGlobals.h"
#include "Systems/AbilitySystem/ARPGGameplayAbilityActorInfo.h"

FGameplayAbilityActorInfo* UARPGAbilitySystemGlobals::AllocAbilityActorInfo() const
{
    // 返回自定义的 ActorInfo，内含缓存的 CMC 指针
    return new FARPGGameplayAbilityActorInfo();
}
