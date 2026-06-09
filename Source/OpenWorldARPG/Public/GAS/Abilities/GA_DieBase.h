// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_DieBase.generated.h"

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_DieBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_DieBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

};