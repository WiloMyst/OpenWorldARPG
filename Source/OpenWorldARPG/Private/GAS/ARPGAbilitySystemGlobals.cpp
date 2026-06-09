// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/ARPGAbilitySystemGlobals.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"

FGameplayAbilityActorInfo* UARPGAbilitySystemGlobals::AllocAbilityActorInfo() const
{
    // 返回自定义的 ActorInfo，内含缓存的 CMC 指针
    return new FARPGGameplayAbilityActorInfo();
}
