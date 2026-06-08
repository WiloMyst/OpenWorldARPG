// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WalkBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * @class UGA_WalkBase
 * @brief 慢走技能基类。
 *
 * 架构原则：GA 管意愿和表现，CMC 管物理和运动状态。
 * - GA 负责：施加状态 GE、监听停止事件
 * - CMC 负责：修改/恢复 MaxWalkSpeed
 * - GA 不持有任何物理参数，不越权访问 CMC 的职责
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_WalkBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_WalkBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UFUNCTION()
    void OnStopWalkEventReceived(FGameplayEventData Payload);

    // ==========================================
    // 策划配置项 (意愿和表现层)
    // ==========================================

    /** 慢走状态 GE (挂载 Tag) */
    UPROPERTY(EditDefaultsOnly, Category = "Walk|Config")
    TSubclassOf<UGameplayEffect> WalkStateEffectClass;

    /** 停止慢走的事件 Tag */
    UPROPERTY(EditDefaultsOnly, Category = "Walk|Config")
    FGameplayTag StopWalkEventTag;

private:
    /** 运行时缓存：慢走状态 GE 句柄 */
    FActiveGameplayEffectHandle ActiveWalkEffectHandle;

    /** 运行时缓存：等待停止事件的异步任务 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
