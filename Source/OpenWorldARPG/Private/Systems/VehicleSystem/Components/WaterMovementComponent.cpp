// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Components/WaterMovementComponent.h"

UWaterMovementComponent::UWaterMovementComponent()
{
}

void UWaterMovementComponent::ClearInput()
{
    ThrottleInput = 0.0f;
    RudderInput = 0.0f;
}

float UWaterMovementComponent::GetSpeedRatio() const
{
    if (MaxForwardSpeed < KINDA_SMALL_NUMBER) return 0.0f;
    return FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxForwardSpeed, 0.0f, 1.0f);
}

// ============================================================================
// 水面采样
// ============================================================================

float UWaterMovementComponent::SampleWaterSurface(FVector WorldLocation) const
{
    // 从上方向下射线检测水面
    const FVector TraceStart(WorldLocation.X, WorldLocation.Y, WorldLocation.Z + 5000.0f);
    const FVector TraceEnd(WorldLocation.X, WorldLocation.Y, WorldLocation.Z - 500.0f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.bTraceComplex = false;
    Params.AddIgnoredActor(PawnOwner);

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, WaterTraceChannel.GetValue(), Params))
    {
        return Hit.Location.Z;
    }

    return DefaultWaterLevel;
}

void UWaterMovementComponent::UpdateWaveResponse(float DeltaTime)
{
    if (!bEnableWaveResponse || !PawnOwner || !UpdatedComponent) return;

    WaveSampleTimer += DeltaTime;
    if (WaveSampleTimer < WaveSampleInterval) return;
    WaveSampleTimer = 0.0f;

    const FVector ShipLoc = UpdatedComponent->GetComponentLocation();
    const FVector Forward = UpdatedComponent->GetForwardVector();
    const FVector Right = UpdatedComponent->GetRightVector();

    // 四点采样水面高度
    const float BowZ = SampleWaterSurface(ShipLoc + Forward * HullLength * 0.5f);
    const float SternZ = SampleWaterSurface(ShipLoc - Forward * HullLength * 0.5f);
    const float PortZ = SampleWaterSurface(ShipLoc - Right * HullWidth * 0.5f);
    const float StarboardZ = SampleWaterSurface(ShipLoc + Right * HullWidth * 0.5f);

    // 计算俯仰 (船头高=正) 和横滚 (右舷高=正)
    const float RawPitch = FMath::RadiansToDegrees(FMath::Atan2(BowZ - SternZ, HullLength));
    const float RawRoll = FMath::RadiansToDegrees(FMath::Atan2(StarboardZ - PortZ, HullWidth));

    // 平滑插值
    const float SmoothAlpha = FMath::Clamp(DeltaTime * WaveTiltSmoothSpeed, 0.0f, 1.0f);
    WavePitch = FMath::Lerp(WavePitch, RawPitch, SmoothAlpha);
    WaveRoll = FMath::Lerp(WaveRoll, RawRoll, SmoothAlpha);
}

void UWaterMovementComponent::UpdateBuoyancy(float DeltaTime, float TargetWaterZ)
{
    if (!UpdatedComponent) return;

    const float TargetZ = TargetWaterZ - Draft;
    const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;

    // 弹簧-阻尼系统
    const float Displacement = TargetZ - CurrentZ;
    const float SpringForce = Displacement * BuoyancyStiffness;
    const float DampingForce = -CurrentVerticalVelocity * BuoyancyDamping;

    CurrentVerticalVelocity += (SpringForce + DampingForce) * DeltaTime;

    // 应用 Z 位移
    FVector CurrentLoc = UpdatedComponent->GetComponentLocation();
    CurrentLoc.Z += CurrentVerticalVelocity * DeltaTime;
    UpdatedComponent->SetWorldLocation(CurrentLoc, false);
}

// ============================================================================
// 主物理更新
// ============================================================================

void UWaterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!PawnOwner || !UpdatedComponent) return;

    // --- 1. 水面检测 ---
    const FVector ShipLoc = UpdatedComponent->GetComponentLocation();
    const float WaterZ = SampleWaterSurface(ShipLoc);
    bIsInWater = (WaterZ > -WORLD_MAX / 2.0f); // 检测到水面

    if (!bIsInWater)
    {
        // 不在水中：不移动
        CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, 1.0f);
        return;
    }

    // --- 2. 浮力更新 ---
    UpdateBuoyancy(DeltaTime, WaterZ);

    // --- 3. 波浪响应 ---
    UpdateWaveResponse(DeltaTime);

    // --- 4. 速度更新 ---
    const float TargetSpeed = ThrottleInput >= 0.0f
        ? ThrottleInput * MaxForwardSpeed
        : ThrottleInput * MaxReverseSpeed;

    if (FMath::Abs(TargetSpeed) > FMath::Abs(CurrentSpeed))
    {
        // 加速
        CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, Acceleration / 100.0f);
    }
    else
    {
        // 减速（水阻）
        CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, WaterDrag / 100.0f);
    }

    // --- 5. 舵转向（需要速度才能转） ---
    if (FMath::Abs(CurrentSpeed) > MinTurnSpeed)
    {
        // 转向速率与速度成正比
        const float SpeedFactor = FMath::Clamp(
            FMath::Abs(CurrentSpeed) / MaxForwardSpeed, 0.0f, 1.0f);
        const float YawDelta = RudderInput * MaxTurnRate * SpeedFactor * DeltaTime;

        // 倒退时转向反转（模拟真实船舶舵效）
        const float DirectionSign = CurrentSpeed >= 0.0f ? 1.0f : -1.0f;

        UpdatedComponent->AddWorldRotation(
            FRotator(0.0f, YawDelta * DirectionSign, 0.0f),
            false, nullptr, ETeleportType::None);
    }

    // --- 6. 水平位移 ---
    const FVector Forward = UpdatedComponent->GetForwardVector();
    FVector MoveDelta = Forward * CurrentSpeed * DeltaTime;

    FHitResult Hit;
    SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

    if (Hit.bBlockingHit)
    {
        // 撞击减速
        CurrentSpeed *= 0.3f;
    }

    // --- 7. 更新 Velocity ---
    Velocity = Forward * CurrentSpeed;
    UpdateComponentVelocity();
}
