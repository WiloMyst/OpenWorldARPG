// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireBase.generated.h"

class UAbilityTask_WaitGameplayEvent;
class APlayerCharacter;
class AWeaponBase;

/**
 * 枪械开火基类 (支持全自动连发)
 * 生命周期：
 * - Activate: 执行第一发，并启动连发定时器。
 * - Loop: 根据 FireRate 每隔一定时间自动执行 PerformSingleShot。
 * - End: 监听到 StopFireEventTag (玩家松开按键) 时，停止定时器并结束。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_FireBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_FireBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 执行单次射击逻辑 (动画、特效、伤害结算) */
    void PerformSingleShot();
    
    /** 结算射线伤害 */
    void ApplyDamage();

    // --- 回调 ---

    /** 监听到玩家松开开火键的事件 */
    UFUNCTION()
    void OnStopFireEventReceived(FGameplayEventData Payload);

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    float FireRate = 0.1f; // 连发间隔（秒）。例如 0.1f = 每秒 10 发。如果是非连发武器，可以设为 0。

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag StopFireEventTag; // 停止开火的 Tag（由控制器在按键 Completed 时发送，如 Character.Event.AimAttackStop）

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> FireMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float MontagePlayRate = 1.4f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    FName MuzzleSocketName = FName("MuzzleSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<AWeaponBase> CachedWeapon;

    /** 监听停止事件的任务 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> StopEventTask;

    /** 连发循环定时器 */
    FTimerHandle FireTimerHandle;
};