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

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 对应图1：死亡状态 GE (例如挂载 Character.State.Dead Tag)
    UPROPERTY(EditDefaultsOnly, Category = "Death|Config")
    TSubclassOf<UGameplayEffect> DeadStateEffectClass;

private:
    // 运行时缓存：保存死亡 GE 句柄，方便在能力结束（例如复活）时移除
    FActiveGameplayEffectHandle ActiveDeadEffectHandle;
};