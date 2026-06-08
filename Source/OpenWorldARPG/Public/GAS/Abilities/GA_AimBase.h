// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AimBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * @class UGA_AimBase
 * @brief 瞄准技能基类。
 *
 * 架构原则：GA 管意愿和表现，CMC 管物理和运动状态。
 * - GA 负责：施加状态 GE、监听停止事件、发送蒙太奇事件
 * - CMC 负责：修改/恢复 MaxWalkSpeed、bOrientRotationToMovement、bUseControllerRotationYaw
 * - GA 不持有任何物理参数，不越权访问 CMC 的职责
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

    // ==========================================
    // 策划配置项 (意愿和表现层)
    // ==========================================

    /** 瞄准状态 GE (挂载 Tag) */
    UPROPERTY(EditDefaultsOnly, Category = "Aim|Config")
    TSubclassOf<UGameplayEffect> AimStateEffectClass;

    /** 停止瞄准的事件 Tag */
    UPROPERTY(EditDefaultsOnly, Category = "Aim|Config")
    FGameplayTag StopAimEventTag;

    /** 瞄准蒙太奇事件 Tag (通知武器组件播放瞄准动画) */
    UPROPERTY(EditDefaultsOnly, Category = "Aim|Config")
    FGameplayTag AimMontageEventTag;

private:
    /** 运行时缓存：瞄准状态 GE 句柄 */
    FActiveGameplayEffectHandle ActiveAimEffectHandle;

    /** 运行时缓存：等待停止事件的异步任务 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
