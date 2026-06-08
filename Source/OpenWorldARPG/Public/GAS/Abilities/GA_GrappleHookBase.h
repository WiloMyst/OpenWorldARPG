// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_GrappleHookBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class APlayerCharacter;
class UNiagaraSystem;
class UNiagaraComponent;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_GrappleHookBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_GrappleHookBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // ==========================================
    // 核心流程 API
    // ==========================================

    /** 对应蓝图：角色转向 (Orient to Hook) */
    void OrientToTarget();

    /** 对应蓝图：绳索特效 (Spawn & Setup Niagara) */
    void TriggerGrappleVFX();

    // ==========================================
    // 回调函数
    // ==========================================

    /** 对应蓝图：Wait Delay 完成后执行位移 */
    UFUNCTION()
    void OnDelayFinished();

    /** 对应蓝图：MoveComponentTo 完成后执行 */
    UFUNCTION()
    void OnMoveCompleted();

    /** 蒙太奇被打断或自然结束时的兜底处理 */
    UFUNCTION()
    void OnMontageFinished();

protected:
    // ==========================================
    // 策划配置项 (Config)
    // ==========================================

    // --- 状态 GE ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> GrapplingStateEffectClass;

    // --- 动画 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> GrappleMontage;

    // --- 移动与时间参数 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float HookDelay = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float GrappleMoveSpeed = 2500.0f;

    // --- 特效 (Niagara) ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    TObjectPtr<UNiagaraSystem> RopeVFXTemplate;

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName AttachSocketName = FName("Right-wrist");

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName RopeEndParamName = FName("End");

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName RopeLifetimeParamName = FName("Lifetime");

private:
    // ==========================================
    // 运行时缓存与状态
    // ==========================================

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<AActor> CurrentHookTarget;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> SpawnedRopeVFX;

    FActiveGameplayEffectHandle GrapplingStateEffectHandle;

    // 任务指针
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitDelay> DelayTask;
};