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
    // --- 核心流程 ---

    void ExecuteAttack();
    void ApplyDamage();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> FireMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float MontagePlayRate = 1.4f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Weapon")
    float FireRange = 5000.0f;

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

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};