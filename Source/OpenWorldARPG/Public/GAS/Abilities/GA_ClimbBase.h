// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ClimbBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_ClimbBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ClimbBase();

    // ==========================================
    // GA 核心生命周期
    // ==========================================

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 对应图2：接收到 Stop Event 时的回调
    UFUNCTION()
    void OnStopClimbEventReceived(FGameplayEventData Payload);

protected:
    // 对应图1：攀爬状态 GE (用于赋予 Character.State.Climbing Tag)
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    TSubclassOf<UGameplayEffect> ClimbingStateEffectClass;

    // 对应图2：触发停止攀爬的 Event Tag (例如: Input.Action.Climb.Stop)
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    FGameplayTag StopClimbEventTag;

private:
    // 运行时缓存：保存施加的 GE 句柄，方便退出时移除
    FActiveGameplayEffectHandle ActiveClimbEffectHandle;

    // 异步任务指针：用于等待停止事件
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};