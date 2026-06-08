// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class APlayerCharacter;
class AWeaponBase;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_FireBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_FireBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // ==========================================
    // 核心流程 API
    // ==========================================

    /** 执行开火核心逻辑 */
    void ExecuteAttack();

    /** 对应蓝图：应用伤害 (两次射线法) */
    void ApplyDamage();

    // ==========================================
    // 回调函数
    // ==========================================

    UFUNCTION()
    void OnMontageFinished();

protected:
    // ==========================================
    // 策划配置项 (Config)
    // ==========================================

    // --- 动画 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> FireMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float MontagePlayRate = 1.4f;

    // --- 武器与射线参数 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    float FireRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    FName MuzzleSocketName = FName("MuzzleSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    // --- 伤害与特效 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
    // ==========================================
    // 运行时缓存
    // ==========================================

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<AWeaponBase> CachedWeapon;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};