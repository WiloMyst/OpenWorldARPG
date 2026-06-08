// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/WeaponBase.h"
#include "GunBase.generated.h"

class UNiagaraSystem;
class USoundBase;
class USoundAttenuation;
class UCameraShakeBase;

UCLASS(Abstract) // 声明为抽象类，供具体的枪械蓝图(如 BP_Rifle)继承
class OPENWORLDARPG_API AGunBase : public AWeaponBase
{
    GENERATED_BODY()

public:
    AGunBase();

    // ==========================================
    // 供 GA 调用的数据获取接口
    // ==========================================

    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    float GetFireRange() const { return FireRange; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    FName GetMuzzleSocketName() const { return MuzzleSocketName; }

    /** 获取枪口的真实世界位置与旋转 */
    UFUNCTION(BlueprintPure, Category = "Weapon|Gun")
    FTransform GetMuzzleTransform() const;

    // ==========================================
    // 供 GA 调用的表现接口
    // ==========================================

    /** 对应你蓝图里的 On Shoot FX 节点 */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Gun")
    virtual void PlayShootFX();

protected:
    // ==========================================
    // 策划配置项：枪械核心数据
    // ==========================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Gun")
    float FireRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Gun")
    FName MuzzleSocketName = FName("MuzzleSocket");

    // ==========================================
    // 策划配置项：开火视听表现
    // ==========================================

    /** 枪口火焰 (Muzzle Flash) 粒子特效 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

    /** 开火音效 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<USoundBase> FireSound;

    /** 开火音效空间衰减配置 (Attenuation Settings) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TObjectPtr<USoundAttenuation> FireSoundAttenuation;

    /** 开火摄像机震动 (后坐力表现) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Effects")
    TSubclassOf<UCameraShakeBase> FireCameraShakeClass;
};