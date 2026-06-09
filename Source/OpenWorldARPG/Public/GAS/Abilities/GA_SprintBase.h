// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SprintBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 极速跑技能基类。由 GA_Dash 结束时检测条件后自动触发，或通过 SprintStart 事件触发。
 *
 * GA 管意愿和表现，CMC 管物理参数。
 * - 激活时应用持续扣减体力的 GameplayEffect
 * - 持续检测移动输入是否归零、体力是否耗尽、Shift 是否释放
 * - 任一条件不满足时自动结束
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

    /** 定时检测移动输入和体力状态 */
    void CheckSprintConditions();

    // --- 配置 ---

    /** 停止极速跑的事件 Tag（Shift 释放时由 Controller 发送） */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    FGameplayTag StopSprintEventTag;

    /** 持续扣减体力的 GameplayEffect 类 */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    TSubclassOf<UGameplayEffect> SprintStaminaDrainGE;

    /** 移动输入归零阈值（输入向量长度平方） */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float MovementInputThreshold = 0.1f;

    /** 体力耗尽阈值，低于此值自动停止极速跑 */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float MinStaminaToSprint = 1.0f;

    /** 状态检测间隔（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float ConditionCheckInterval = 0.1f;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;

    /** 持续扣减体力的 GE Handle */
    FActiveGameplayEffectHandle StaminaDrainGEHandle;

    /** 状态检测定时器 */
    FTimerHandle ConditionCheckTimer;
};
