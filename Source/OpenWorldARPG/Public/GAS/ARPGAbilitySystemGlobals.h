// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "ARPGAbilitySystemGlobals.generated.h"

/**
 * 自定义 AbilitySystemGlobals。重写 AllocAbilityActorInfo 返回带 CMC 缓存的结构体，
 * 使 GA 可直接 StaticCast 读取缓存指针，无需 FindComponentByClass。
 */
UCLASS()
class UARPGAbilitySystemGlobals : public UAbilitySystemGlobals
{
    GENERATED_BODY()

public:
    virtual FGameplayAbilityActorInfo* AllocAbilityActorInfo() const override;
};
