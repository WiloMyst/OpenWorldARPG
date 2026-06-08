// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlungeAttackBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitMovementModeChange;
class APlayerCharacter;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_PlungeAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_PlungeAttackBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // ==========================================
    // 核心流程 API
    // ==========================================

    /** 对应蓝图：Execute Attack (执行攻击) */
    void ExecuteAttack();

    /** 对应蓝图：Apply Damage (施加下落攻击伤害) */
    void ApplyDamageToTargets();

    /** 清理所有的异步任务 */
    void ClearAllTasks();

    void CorrectPawnOrient();

    // ==========================================
    // 回调函数 (响应 Ability Tasks)
    // ==========================================

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnMovementModeChanged(EMovementMode NewMovementMode);

    UFUNCTION()
    void OnDamageEventReceived(FGameplayEventData Payload);

protected:
    // ==========================================
    // 策划配置项 (Config)
    // ==========================================

    // --- 状态与伤害 GE ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> PlungeStateEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // --- 事件 Tags ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

    // --- 命中判定检测 (下落攻击通常为自身半径的球体范围伤害) ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float PlungeDamageRadius = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

private:
    // ==========================================
    // 运行时状态 (Runtime State)
    // ==========================================

    /** 对应蓝图：Interrupted by Land (是否因落地而打断) */
    bool bInterruptedByLand = false;

    FActiveGameplayEffectHandle PlungeStateEffectHandle;

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    // ==========================================
    // 缓存的任务指针
    // ==========================================
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitMovementModeChange> MovementModeTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageTask;
};