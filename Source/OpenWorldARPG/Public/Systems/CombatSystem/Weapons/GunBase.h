// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/CombatSystem/Weapons/WeaponBase.h"
#include "GunBase.generated.h"

class UParticleSystem;
class USoundBase;
class USoundAttenuation;
class UCameraShakeBase;

UCLASS(Abstract)
class OPENWORLDARPG_API AGunBase : public AWeaponBase
{
    GENERATED_BODY()

public:
    AGunBase();

    // --- 数据接口 (供 GA 调用) ---

    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    float GetFireRange() const { return FireRange; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    FName GetMuzzleSocketName() const { return MuzzleSocketName; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    FTransform GetMuzzleTransform() const;

    // --- 表现接口 (供 GA 调用) ---
    UFUNCTION(BlueprintCallable, Category = "Weapon|Gun")
    virtual void PlayShootFX();

protected:
    // --- 配置：枪械数据 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Gun")
    float FireRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Gun")
    FName MuzzleSocketName = FName("MuzzleSocket");

    // --- 配置：开火视听表现 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<UParticleSystem> MuzzleFlashEffect;

    // 枪口火焰的缩放大小
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    FVector MuzzleFlashScale = FVector(1.0f, 1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<USoundAttenuation> FireSoundAttenuation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TSubclassOf<UCameraShakeBase> FireCameraShakeClass;
};