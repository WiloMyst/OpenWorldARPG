// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/AttackTypes.h"
#include "GA_MeleeAttackBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class APlayerCharacter;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_MeleeAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_MeleeAttackBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // ==========================================
    // 核心流程 API
    // ==========================================

    /** 执行攻击 */
    void ExecuteAttack();

    /** 攻击转向 */
    void AttackOrientation();

    /** Apply Damage (球体追踪与施加伤害 GE) */
    void ApplyDamageToTargets();

    /** 清理所有的异步任务，防止连招打断时互相干扰 */
    void ClearAllTasks();

    /** 修正 Pawn 朝向 */
    void CorrectPawnOrient();

    // ==========================================
    // 回调函数 (响应 Ability Tasks)
    // ==========================================

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnDamageEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnComboOpenEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnComboCloseEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnNextAttackInputReceived(FGameplayEventData Payload);

protected:
    // ==========================================
    // 策划配置项 (Config)
    // ==========================================

    // 攻击类型
    UPROPERTY(EditDefaultsOnly, Category = "Config|Type")
    EAttackType AttackType = EAttackType::Normal;

    // --- 状态与伤害 GE ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> AttackingStateEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // --- 事件 Tags ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowOpenTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowCloseTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag NextAttackInputTag;

    /** 自动索敌检测半径 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    float OrientRadius = 400.0f;

    /** 索敌检测的对象类型 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    TArray<TEnumAsByte<EObjectTypeQuery>> OrientObjectTypes;

    /** 敌人的 Actor 标签，用于筛选 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    FName EnemyActorTag = FName("Enemy");

    // --- 命中判定检测 (Trace) ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceRadius = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceForwardOffset1 = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceForwardOffset2 = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

private:
    // ==========================================
    // 运行时状态 (Runtime State)
    // ==========================================

    int32 ComboIndex = 0;
    bool bCanTriggerAttack = true;

    // 记录已经受击的敌人，防止一次挥砍造成多次伤害
    UPROPERTY()
    TArray<AActor*> HitActors;

    FActiveGameplayEffectHandle AttackingStateEffectHandle;

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    // ==========================================
    // 缓存的任务指针
    // ==========================================
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboOpenTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboCloseTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> InputTask;
};