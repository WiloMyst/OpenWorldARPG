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

protected:
    virtual void BeginPlay() override;

public:
    /** 播放武器生成特效（生成遮罩 + 整体光亮 + 花纹光亮） */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Effects")
    void PlayWeaponSpawnFX();

    /** 播放武器消散特效（消散遮罩 + Niagara 粒子） */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Effects")
    void PlayWeaponDissolveFX();

protected:
    UFUNCTION()
    void WeaponSpawnEvent();

    UFUNCTION()
    void WeaponDissolveEvent();

    // ---- 组件 ----

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UStaticMeshComponent> WeaponMesh;

    /** 驱动材质参数 "生成遮罩" 的时间轴 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> SpawnMaskTimeline;

    /** 驱动材质参数 "整体光亮" 的时间轴 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> OverallBrightnessTimeline;

    /** 驱动材质参数 "花纹光亮" 的时间轴 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> PatternBrightnessTimeline;

    /** 驱动材质参数 "消散遮罩" 的时间轴 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
    TObjectPtr<UTimelineComponent> DissolveMaskTimeline;

    // ---- 材质 ----

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Material")
    TObjectPtr<UMaterialInterface> WeaponSourceMaterial;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Material")
    TObjectPtr<UMaterialInstanceDynamic> WeaponMaterialDynamicInstance;

    // ---- 粒子 ----

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
    TObjectPtr<UNiagaraSystem> DissolveNiagaraSystem;

    // ---- 曲线 (每个 Timeline 一条曲线，时长与缓动独立) ----

    /** 驱动材质参数 "生成遮罩"，控制武器从无到有的遮罩范围 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> SpawnMaskCurve;

    /** 驱动材质参数 "整体光亮"，控制武器生成时的整体亮度渐变 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> OverallBrightnessCurve;

    /** 驱动材质参数 "花纹光亮"，控制武器生成时的花纹亮度渐变 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|SpawnCurves")
    TObjectPtr<UCurveFloat> PatternBrightnessCurve;

    /** 驱动材质参数 "消散遮罩"，控制武器从有到无的消散范围 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects|DissolveCurves")
    TObjectPtr<UCurveFloat> DissolveMaskCurve;

private:
    UFUNCTION() void UpdateSpawnMask(float Value);
    UFUNCTION() void UpdateOverallBrightness(float Value);
    UFUNCTION() void UpdatePatternBrightness(float Value);
    UFUNCTION() void UpdateDissolveMask(float Value);

    FOnTimelineFloat SpawnMaskCallback;
    FOnTimelineFloat OverallBrightnessCallback;
    FOnTimelineFloat PatternBrightnessCallback;
    FOnTimelineFloat DissolveMaskCallback;
};
