// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_JumpBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_JumpBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_JumpBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UFUNCTION()
    void OnStopJumpEventReceived(FGameplayEventData Payload);

    // ==========================================
    // 策划配置项 (取代硬编码)
    // ==========================================

    // 对应蓝图：停止跳跃的事件 Tag (Input.Action.Jump.Stop)
    UPROPERTY(EditDefaultsOnly, Category = "Jump|Config")
    FGameplayTag StopJumpEventTag;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};