// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Components/HoverMovementComponent.h"

UHoverMovementComponent::UHoverMovementComponent()
{
}

void UHoverMovementComponent::ClearInput()
{
    ForwardInput = 0.0f;
    StrafeInput = 0.0f;
    ThrottleInput = 0.0f;
    YawInput = 0.0f;
    bIsPrecision = false;
}

float UHoverMovementComponent::GetCurrentAltitude() const
{
    if (PawnOwner) return PawnOwner->GetActorLocation().Z;
    return 0.0f;
}

bool UHoverMovementComponent::IsHovering() const
{
    return CurrentHorizontalVelocity.SizeSquared2D() < 100.0f &&
           FMath::Abs(CurrentVerticalVelocity) < 50.0f;
}

void UHoverMovementComponent::SimulateMovement(float DeltaTime)
{
    const float SpeedMul = bIsPrecision ? PrecisionSpeedMultiplier : 1.0f;

    // --- 1. 偏航旋转 ---
    const float YawDelta = YawInput * YawRate * DeltaTime;
    UpdatedComponent->AddWorldRotation(FRotator(0.0f, YawDelta, 0.0f), false, nullptr, ETeleportType::None);

    // --- 2. 水平速度计算 ---
    const FVector Forward = UpdatedComponent->GetForwardVector();
    const FVector Right = UpdatedComponent->GetRightVector();

    const FVector InputDir = (Forward * ForwardInput + Right * StrafeInput).GetSafeNormal2D();
    const float InputMag = FMath::Clamp(FVector2D(ForwardInput, StrafeInput).Size(), 0.0f, 1.0f);

    const FVector TargetHorizontalVel = InputDir * MaxHorizontalSpeed * SpeedMul * InputMag;

    if (TargetHorizontalVel.SizeSquared2D() > CurrentHorizontalVelocity.SizeSquared2D())
    {
        // 加速
        CurrentHorizontalVelocity = FMath::VInterpTo(
            CurrentHorizontalVelocity, TargetHorizontalVel, DeltaTime, Acceleration / 100.0f);
    }
    else
    {
        // 减速
        CurrentHorizontalVelocity = FMath::VInterpTo(
            CurrentHorizontalVelocity, TargetHorizontalVel, DeltaTime, Deceleration / 100.0f);
    }

    // --- 3. 垂直速度计算 ---
    float TargetVerticalVel = ThrottleInput * MaxVerticalSpeed * SpeedMul;

    // 悬停模式：无油门输入时自动保持高度
    if (FMath::Abs(ThrottleInput) < HoverDeadzone)
    {
        // 逐渐归零垂直速度（抵消重力）
        TargetVerticalVel = 0.0f;
        CurrentVerticalVelocity = FMath::FInterpTo(
            CurrentVerticalVelocity, 0.0f, DeltaTime, HoverGravityCompensation / 100.0f);
    }
    else
    {
        CurrentVerticalVelocity = FMath::FInterpTo(
            CurrentVerticalVelocity, TargetVerticalVel, DeltaTime, Acceleration / 100.0f);
    }

    // --- 4. 视觉倾斜计算（纯视觉，不影响移动） ---
    const float TargetTiltPitch = -ForwardInput * MaxTiltAngle * SpeedMul;
    const float TargetTiltRoll = StrafeInput * MaxTiltAngle * SpeedMul;

    TiltPitch = FMath::FInterpTo(TiltPitch, TargetTiltPitch, DeltaTime, TiltRecoverySpeed);
    TiltRoll = FMath::FInterpTo(TiltRoll, TargetTiltRoll, DeltaTime, TiltRecoverySpeed);

    // --- 5. 执行移动 ---
    FVector MoveDelta = CurrentHorizontalVelocity * DeltaTime;
    MoveDelta.Z = CurrentVerticalVelocity * DeltaTime;

    FHitResult Hit;
    SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

    // 碰撞处理
    if (Hit.bBlockingHit)
    {
        // 沿碰撞面滑动
        CurrentHorizontalVelocity = FVector::VectorPlaneProject(
            CurrentHorizontalVelocity, Hit.Normal) * 0.5f;

        if (Hit.Normal.Z > 0.7f) // 撞地面/天花板
        {
            CurrentVerticalVelocity = 0.0f;
        }
    }

    // --- 6. 更新 Velocity（供网络复制） ---
    Velocity = CurrentHorizontalVelocity;
    Velocity.Z = CurrentVerticalVelocity;
    UpdateComponentVelocity();
}
