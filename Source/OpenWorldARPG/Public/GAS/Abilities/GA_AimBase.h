// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AimBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 瞄准技能基类。GA 管意愿和表现，CMC 管物理参数。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_AimBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AimBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UFUNCTION()
    void OnStopAimEventReceived(FGameplayEventData Payload);

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Aim|Config")
    FGameplayTag StopAimEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Aim|Config")
    FGameplayTag AimMontageEventTag;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
