// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "WeaponBase.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UCurveFloat;

UCLASS()
class OPENWORLDARPG_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AWeaponBase();

    /** 播放武器生成特效（生成遮罩 + 整体光亮 + 花纹光亮） */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Effects")
    void PlayWeaponSpawnFX();

    /** 播放武器消散特效（消散遮罩 + Niagara 粒子） */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Effects")
    void PlayWeaponDissolveFX();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void WeaponSpawnEvent();

    UFUNCTION()
    void WeaponDissolveEvent();

private:
    UFUNCTION() void UpdateSpawnMask(float Value);
    UFUNCTION() void UpdateOverallBrightness(float Value);
    UFUNCTION() void UpdatePatternBrightness(float Value);
    UFUNCTION() void UpdateDissolveMask(float Value);

protected:
    // --- 组件 ---

    /** 武器网格组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UStaticMeshComponent> WeaponMesh;

    /** 武器生成罩线组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> SpawnMaskTimeline;

    /** 武器整体光亮线线组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> OverallBrightnessTimeline;

    /** 武器纹光亮线线组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> PatternBrightnessTimeline;

    /** 武器消散罩线组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> DissolveMaskTimeline;

    // --- 材质 ---

    /** 武器源材质组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Material")
    TObjectPtr<UMaterialInterface> WeaponSourceMaterial;

    /** 武器材质动态实例组件 */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Material")
    TObjectPtr<UMaterialInstanceDynamic> WeaponMaterialDynamicInstance;

    // --- 粒子 ---

    /** 武器消散 Niagara 粒子系统组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
    TObjectPtr<UNiagaraSystem> DissolveNiagaraSystem;

    // --- 曲线 ---

    /** 武器生成罩线曲线组件 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> SpawnMaskCurve;

    /** 武器整体光亮线曲线组件 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> OverallBrightnessCurve;

    /** 武器纹光亮线曲线组件 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> PatternBrightnessCurve;

    /** 武器消散罩线曲线组件 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|DissolveCurves")
    TObjectPtr<UCurveFloat> DissolveMaskCurve;

private:
    FOnTimelineFloat SpawnMaskCallback;
    FOnTimelineFloat OverallBrightnessCallback;
    FOnTimelineFloat PatternBrightnessCallback;
    FOnTimelineFloat DissolveMaskCallback;
};
