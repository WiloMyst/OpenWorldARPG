// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SprintBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 冲刺技能基类。GA 管意愿和表现，CMC 管物理参数。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SprintBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SprintBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UFUNCTION()
    void OnStopSprintEventReceived(FGameplayEventData Payload);

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    FGameplayTag StopSprintEventTag;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
