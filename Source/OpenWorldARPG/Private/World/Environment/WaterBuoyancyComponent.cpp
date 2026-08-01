// Copyright 2025 WiloMyst. All Rights Reserved.

#include "World/Environment/WaterBuoyancyComponent.h"
#include "World/Environment/WaterVolume.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UWaterBuoyancyComponent::UWaterBuoyancyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // 默认 4 个浮力采样点（物体底部四角）
    InitDefaultBuoyancyPoints();
}

void UWaterBuoyancyComponent::InitDefaultBuoyancyPoints()
{
    if (BuoyancyPoints.Num() == 0)
    {
        BuoyancyPoints = {
            FVector( 50.0f,  50.0f, 0.0f),
            FVector(-50.0f,  50.0f, 0.0f),
            FVector( 50.0f, -50.0f, 0.0f),
            FVector(-50.0f, -50.0f, 0.0f)
        };
    }
}

AWaterVolume* UWaterBuoyancyComponent::FindOverlappingWaterVolume() const
{
    if (!GetOwner()) return nullptr;

    TArray<AActor*> OverlappingActors;
    GetOwner()->GetOverlappingActors(OverlappingActors, AWaterVolume::StaticClass());

    for (AActor* Actor : OverlappingActors)
    {
        if (AWaterVolume* WaterVolume = Cast<AWaterVolume>(Actor))
        {
            return WaterVolume;
        }
    }
    return nullptr;
}

void UWaterBuoyancyComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 获取物理组件
    UPrimitiveComponent* PrimComp = Owner->FindComponentByClass<UPrimitiveComponent>();
    if (!PrimComp || !PrimComp->IsSimulatingPhysics()) return;

    // 查找水体
    AWaterVolume* WaterVolume = FindOverlappingWaterVolume();
    if (!WaterVolume) return;

    const float WaterSurfaceZ = WaterVolume->GetWaterSurfaceZ();
    const FTransform OwnerTransform = Owner->GetActorTransform();

    // --- 在每个浮力采样点施加向上的力 ---
    for (const FVector& LocalPoint : BuoyancyPoints)
    {
        FVector WorldPoint = OwnerTransform.TransformPosition(LocalPoint);
        const float SubmersionDepth = WaterSurfaceZ - WorldPoint.Z;

        if (SubmersionDepth > 0.0f)
        {
            // 浸没深度越大，浮力越大（线性模型）
            float ForceMagnitude = FMath::Min(
                SubmersionDepth * BuoyancyStrength, MaxBuoyancyForce);

            PrimComp->AddForceAtLocation(
                FVector(0.0f, 0.0f, ForceMagnitude),
                WorldPoint);
        }
    }

    // --- 水流推力 ---
    const FVector CurrentForce = WaterVolume->GetCurrentForce();
    if (!CurrentForce.IsNearlyZero())
    {
        PrimComp->AddForce(CurrentForce * PrimComp->GetMass() * 0.3f);
    }

    // --- 水阻尼 ---
    PrimComp->SetLinearDamping(WaterLinearDamping);
    PrimComp->SetAngularDamping(WaterAngularDamping);
}
