// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Components/FlightMovementComponent.h"

UFlightMovementComponent::UFlightMovementComponent()
{
}

void UFlightMovementComponent::ClearInput()
{
    ThrottleInput = 0.0f;
    PitchInput = 0.0f;
    RollInput = 0.0f;
    YawInput = 0.0f;
    bIsBoosting = false;
}

float UFlightMovementComponent::GetSpeedRatio() const
{
    const float MaxSpeed = bIsBoosting ? MaxBoostSpeed : MaxCruiseSpeed;
    if (MaxSpeed < KINDA_SMALL_NUMBER) return 0.0f;
    return FMath::Clamp(CurrentSpeed / MaxSpeed, 0.0f, 1.0f);
}

void UFlightMovementComponent::SimulateMovement(float DeltaTime)
{
    // --- 1. 平滑输入 ---
    const float SmoothAlpha = FMath::Clamp(DeltaTime * InputSmoothSpeed, 0.0f, 1.0f);
    SmoothedPitch = FMath::Lerp(SmoothedPitch, PitchInput, SmoothAlpha);
    SmoothedYaw = FMath::Lerp(SmoothedYaw, YawInput, SmoothAlpha);
    SmoothedRoll = FMath::Lerp(SmoothedRoll, RollInput, SmoothAlpha);

    // --- 2. 速度更新 ---
    const float TargetMaxSpeed = bIsBoosting ? MaxBoostSpeed : MaxCruiseSpeed;
    const float TargetSpeed = FMath::Lerp(0.0f, TargetMaxSpeed, FMath::Max(ThrottleInput, 0.0f));

    if (CurrentSpeed < TargetSpeed)
    {
        CurrentSpeed = FMath::Min(CurrentSpeed + Acceleration * DeltaTime, TargetSpeed);
    }
    else if (CurrentSpeed > TargetSpeed)
    {
        const float Decel = (ThrottleInput < 0.0f) ? Acceleration * 2.0f : Deceleration;
        CurrentSpeed = FMath::Max(CurrentSpeed - Decel * DeltaTime, TargetSpeed);
    }

    // 最低速度不低于 0
    CurrentSpeed = FMath::Max(CurrentSpeed, 0.0f);

    // --- 3. 失速检测 ---
    bIsStalling = (CurrentSpeed < StallSpeed);

    // --- 4. 旋转更新 ---
    float PitchDelta = SmoothedPitch * PitchRate * DeltaTime;
    float YawDelta = SmoothedYaw * YawRate * DeltaTime;
    float RollDelta = SmoothedRoll * RollRate * DeltaTime;

    // 失速时自动下俯
    if (bIsStalling)
    {
        PitchDelta -= StallAutoPitchRate * DeltaTime;
    }

    // 银行转弯：有偏航输入且无手动横滚时自动倾斜
    if (FMath::Abs(SmoothedYaw) > 0.05f && FMath::Abs(SmoothedRoll) < 0.1f)
    {
        CurrentBankAngle = FMath::Lerp(CurrentBankAngle,
            SmoothedYaw * AutoBankMaxAngle, SmoothAlpha);
    }
    else
    {
        // 回正
        CurrentBankAngle = FMath::Lerp(CurrentBankAngle, 0.0f,
            DeltaTime * AutoBankRecoveryRate);
    }

    RollDelta += (CurrentBankAngle - UpdatedComponent->GetComponentRotation().Roll) * DeltaTime * 3.0f;

    // 应用本地空间旋转
    const FQuat DeltaQuat = FRotator(PitchDelta, YawDelta, RollDelta).Quaternion();
    UpdatedComponent->AddLocalRotation(DeltaQuat, false, nullptr, ETeleportType::None);

    // --- 5. 位移计算 ---
    const FVector Forward = UpdatedComponent->GetForwardVector();
    const FVector Up = UpdatedComponent->GetUpVector();
    const FVector WorldUp = FVector::UpVector;

    // 基础位移：沿前方
    FVector MoveDelta = Forward * CurrentSpeed * DeltaTime;

    // 失速/重力
    if (bIsStalling)
    {
        // 重力下坠
        MoveDelta += FVector::DownVector * Gravity * DeltaTime;

        // 速度方向逐渐对齐前方（让机鼻朝下时恢复速度）
        if (Forward.Z < 0.0f)
        {
            CurrentSpeed += -Forward.Z * Gravity * DeltaTime * 0.5f;
        }
    }
    else
    {
        // 正常飞行：升力抵消重力
        // 如果机鼻朝上，速度逐渐降低（爬升消耗动能）
        if (Forward.Z > 0.3f)
        {
            CurrentSpeed -= Forward.Z * Gravity * DeltaTime * 0.3f;
            CurrentSpeed = FMath::Max(CurrentSpeed, 0.0f);
        }
        // 如果机鼻朝下，速度逐渐增加（俯冲加速）
        else if (Forward.Z < -0.1f)
        {
            CurrentSpeed += -Forward.Z * Gravity * DeltaTime * 0.4f;
            CurrentSpeed = FMath::Min(CurrentSpeed, MaxBoostSpeed * 1.2f);
        }
    }

    // --- 6. 执行移动（带碰撞检测） ---
    FHitResult Hit;
    SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

    // 碰撞处理：撞墙减速
    if (Hit.bBlockingHit)
    {
        CurrentSpeed *= 0.3f;
    }

    // --- 7. 更新 Velocity（供网络复制和物理系统） ---
    Velocity = Forward * CurrentSpeed;
    UpdateComponentVelocity();
}
