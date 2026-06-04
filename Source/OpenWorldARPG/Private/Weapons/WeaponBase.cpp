// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Weapons/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = false;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
    WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 4 个独立 Timeline，严格对应蓝图中的 4 个时间轴节点
    SpawnMaskTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("SpawnMaskTimeline"));
    OverallBrightnessTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("OverallBrightnessTimeline"));
    PatternBrightnessTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("PatternBrightnessTimeline"));
    DissolveMaskTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveMaskTimeline"));
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();

    // 创建动态材质实例
    if (WeaponSourceMaterial && WeaponMesh)
    {
        WeaponMaterialDynamicInstance = UMaterialInstanceDynamic::Create(WeaponSourceMaterial, this);
        if (WeaponMaterialDynamicInstance)
        {
            WeaponMesh->SetMaterial(0, WeaponMaterialDynamicInstance);
        }
    }

    // 每个 Timeline 只绑定一条曲线，与蓝图中的独立时间轴节点一一对应
    if (SpawnMaskTimeline && SpawnMaskCurve)
    {
        SpawnMaskCallback.BindDynamic(this, &AWeaponBase::UpdateSpawnMask);
        SpawnMaskTimeline->AddInterpFloat(SpawnMaskCurve, SpawnMaskCallback);
    }

    if (OverallBrightnessTimeline && OverallBrightnessCurve)
    {
        OverallBrightnessCallback.BindDynamic(this, &AWeaponBase::UpdateOverallBrightness);
        OverallBrightnessTimeline->AddInterpFloat(OverallBrightnessCurve, OverallBrightnessCallback);
    }

    if (PatternBrightnessTimeline && PatternBrightnessCurve)
    {
        PatternBrightnessCallback.BindDynamic(this, &AWeaponBase::UpdatePatternBrightness);
        PatternBrightnessTimeline->AddInterpFloat(PatternBrightnessCurve, PatternBrightnessCallback);
    }

    if (DissolveMaskTimeline && DissolveMaskCurve)
    {
        DissolveMaskCallback.BindDynamic(this, &AWeaponBase::UpdateDissolveMask);
        DissolveMaskTimeline->AddInterpFloat(DissolveMaskCurve, DissolveMaskCallback);
    }
}

// ==========================================
// 外部调用接口
// ==========================================

void AWeaponBase::PlayWeaponSpawnFX()
{
    WeaponSpawnEvent();
}

void AWeaponBase::PlayWeaponDissolveFX()
{
    WeaponDissolveEvent();
}

// ==========================================
// 核心事件实现
// ==========================================

void AWeaponBase::WeaponSpawnEvent()
{
    if (!WeaponMaterialDynamicInstance) return;

    // 重置消散遮罩，确保武器可见
    WeaponMaterialDynamicInstance->SetScalarParameterValue(FName("消散遮罩"), 0.0f);

    // 同时播放 3 条生成曲线，各自独立时长
    if (SpawnMaskTimeline) SpawnMaskTimeline->PlayFromStart();
    if (OverallBrightnessTimeline) OverallBrightnessTimeline->PlayFromStart();
    if (PatternBrightnessTimeline) PatternBrightnessTimeline->PlayFromStart();
}

void AWeaponBase::WeaponDissolveEvent()
{
    if (!WeaponMaterialDynamicInstance) return;

    // 播放消散材质动画
    if (DissolveMaskTimeline) DissolveMaskTimeline->PlayFromStart();

    // 生成消散粒子特效
    if (DissolveNiagaraSystem && WeaponMesh)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            DissolveNiagaraSystem,
            WeaponMesh->GetComponentLocation(),
            WeaponMesh->GetComponentRotation(),
            WeaponMesh->GetComponentScale(),
            true,
            true
        );
    }
}

// ==========================================
// Timeline 回调
// ==========================================

void AWeaponBase::UpdateSpawnMask(float Value)
{
    if (WeaponMaterialDynamicInstance)
        WeaponMaterialDynamicInstance->SetScalarParameterValue(FName("生成遮罩"), Value);
}

void AWeaponBase::UpdateOverallBrightness(float Value)
{
    if (WeaponMaterialDynamicInstance)
        WeaponMaterialDynamicInstance->SetScalarParameterValue(FName("整体光亮"), Value);
}

void AWeaponBase::UpdatePatternBrightness(float Value)
{
    if (WeaponMaterialDynamicInstance)
        WeaponMaterialDynamicInstance->SetScalarParameterValue(FName("花纹光亮"), Value);
}

void AWeaponBase::UpdateDissolveMask(float Value)
{
    if (WeaponMaterialDynamicInstance)
        WeaponMaterialDynamicInstance->SetScalarParameterValue(FName("消散遮罩"), Value);
}
