// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Components/VehicleAutopilotComponent.h"
#include "Systems/VehicleSystem/Pawns/WheeledVehiclePawnBase.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DrawDebugHelpers.h"

UVehicleAutopilotComponent::UVehicleAutopilotComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVehicleAutopilotComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AWheeledVehiclePawnBase* Vehicle = GetVehicleOwner())
    {
        CachedVehicleMovement = Vehicle->GetWheeledVehicleMovement();
    }
}

// ============================================================================
// 公开接口
// ============================================================================

bool UVehicleAutopilotComponent::StartAutopilot(const TArray<FVector>& InPathPoints, float InTargetSpeed)
{
    if (InPathPoints.Num() < 2) return false;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!CachedVehicleMovement) return false;

    PathPoints = InPathPoints;
    CurrentPathIndex = 0;
    LookAheadPoint = InPathPoints[0];
    LookAheadIndex = 0;
    SpeedIntegral = 0.0f;
    PrevSpeedError = 0.0f;
    CurrentTargetSpeed = (InTargetSpeed > 0.0f) ? InTargetSpeed : DefaultCruiseSpeed;
    CurrentState = EVehicleAutopilotState::Starting;

    SetComponentTickEnabled(true);
    return true;
}

void UVehicleAutopilotComponent::StopAutopilot()
{
    CurrentState = EVehicleAutopilotState::Idle;
    PathPoints.Reset();
    CurrentPathIndex = 0;
    SpeedIntegral = 0.0f;
    PrevSpeedError = 0.0f;

    if (CachedVehicleMovement)
    {
        CachedVehicleMovement->SetThrottleInput(0.0f);
        CachedVehicleMovement->SetSteeringInput(0.0f);
        CachedVehicleMovement->SetBrakeInput(0.0f);
    }

    SetComponentTickEnabled(false);
}

// ============================================================================
// Tick: 主控制循环
// ============================================================================

void UVehicleAutopilotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 仅服务器端运行
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!CachedVehicleMovement || PathPoints.Num() == 0) return;

    // 1. 更新当前路径索引
    CurrentPathIndex = FindClosestPathIndex();

    // 2. 更新前瞻点
    UpdateLookAheadPoint();

    // 3. 避障检测
    const float ObstacleDist = CheckObstacleAhead();

    // 4. 状态机更新
    UpdateStateMachine(DeltaTime, ObstacleDist);

    // 5. Debug 绘制
    if (bDrawDebug)
    {
        const FVector VehicleLoc = GetVehicleLocation();

        // 绘制剩余路径
        for (int32 i = CurrentPathIndex; i < PathPoints.Num() - 1; ++i)
        {
            DrawDebugLine(GetWorld(), PathPoints[i], PathPoints[i + 1],
                FColor::Green, false, -1.0f, SDPG_World, 3.0f);
        }

        // 绘制前瞻点
        DrawDebugSphere(GetWorld(), LookAheadPoint, 25.0f, 8,
            FColor::Yellow, false, -1.0f, SDPG_World);

        // 绘制车辆到前瞻点连线
        DrawDebugLine(GetWorld(), VehicleLoc, LookAheadPoint,
            FColor::Cyan, false, -1.0f, SDPG_World, 2.0f);
    }

    // 6. 到达或空闲时不输出控制
    if (CurrentState == EVehicleAutopilotState::Idle ||
        CurrentState == EVehicleAutopilotState::Arrived)
    {
        return;
    }

    // 7. 计算并应用转向
    const float SteeringInput = ComputeSteering();
    CachedVehicleMovement->SetSteeringInput(SteeringInput);

    // 8. 计算并应用油门/刹车
    float ThrottleInput = 0.0f;
    float BrakeInput = 0.0f;
    ComputeThrottleAndBrake(DeltaTime, ThrottleInput, BrakeInput);
    CachedVehicleMovement->SetThrottleInput(ThrottleInput);
    CachedVehicleMovement->SetBrakeInput(BrakeInput);
}

// ============================================================================
// 核心算法: Pure Pursuit 转向控制
// ============================================================================

float UVehicleAutopilotComponent::ComputeSteering() const
{
    AWheeledVehiclePawnBase* Vehicle = GetVehicleOwner();
    if (!Vehicle) return 0.0f;

    const FVector VehicleLoc = GetVehicleLocation();
    const FRotator VehicleRot = Vehicle->GetActorRotation();

    // 目标点转到车辆本地坐标（X=前, Y=右, Z=上）
    const FVector ToTarget = LookAheadPoint - VehicleLoc;
    const FVector LocalTarget = VehicleRot.UnrotateVector(ToTarget);

    // 目标方向与车辆朝向的夹角
    const float Alpha = FMath::Atan2(LocalTarget.Y, LocalTarget.X);

    // 2D 距离
    const float Ld = FMath::Sqrt(LocalTarget.X * LocalTarget.X + LocalTarget.Y * LocalTarget.Y);
    if (Ld < KINDA_SMALL_NUMBER) return 0.0f;

    // Pure Pursuit 公式: delta = atan(2 * L * sin(alpha) / Ld)
    const float SteeringAngleRad = FMath::Atan2(
        2.0f * Wheelbase * FMath::Sin(Alpha), Ld);

    // 获取最大转向角（从车辆配置）
    float MaxSteerDeg = 70.0f;
    if (Vehicle->VehicleConfig)
    {
        MaxSteerDeg = Vehicle->VehicleConfig->MaxSteeringAngle;
    }
    const float MaxSteerRad = FMath::DegreesToRadians(MaxSteerDeg);

    // 归一化到 -1 ~ 1（Chaos VehicleMovement 的输入范围）
    return FMath::Clamp(SteeringAngleRad / MaxSteerRad, -1.0f, 1.0f);
}

// ============================================================================
// 核心算法: PID 速度控制
// ============================================================================

void UVehicleAutopilotComponent::ComputeThrottleAndBrake(float DeltaTime, float& OutThrottle, float& OutBrake)
{
    const float CurrentSpeed = GetVehicleSpeed(); // cm/s, 正=前进
    const float Error = CurrentTargetSpeed - CurrentSpeed;

    // P: 比例
    const float P = PID_Kp * Error;

    // I: 积分（带抗饱和）
    SpeedIntegral = FMath::Clamp(
        SpeedIntegral + Error * DeltaTime,
        -PID_IntegralClamp, PID_IntegralClamp);
    const float I = PID_Ki * SpeedIntegral;

    // D: 微分（误差变化率）
    float D = 0.0f;
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        D = PID_Kd * (Error - PrevSpeedError) / DeltaTime;
    }
    PrevSpeedError = Error;

    const float Output = P + I + D;

    // 正数 = 油门, 负数 = 刹车
    OutThrottle = FMath::Clamp(Output, 0.0f, 1.0f);
    OutBrake = FMath::Clamp(-Output, 0.0f, 1.0f);
}

// ============================================================================
// 避障检测: 前方三射线
// ============================================================================

float UVehicleAutopilotComponent::CheckObstacleAhead() const
{
    AWheeledVehiclePawnBase* Vehicle = GetVehicleOwner();
    if (!Vehicle || !GetWorld()) return -1.0f;

    const FVector Start = GetVehicleLocation();
    const FVector Forward = GetVehicleForward();
    const FVector Right = Vehicle->GetActorRightVector();

    // 估算车辆宽度
    float VehicleWidth = 200.0f;
    if (Vehicle->GetVehicleMesh())
    {
        const FBox Box = Vehicle->GetVehicleMesh()->Bounds.GetBox();
        VehicleWidth = Box.GetSize().Y;
    }

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Vehicle);

    float MinDistance = -1.0f;

    // 三条射线: 左、中、右
    for (float Offset : {-0.4f, 0.0f, 0.4f})
    {
        const FVector RayStart = Start + Right * (Offset * VehicleWidth);
        const FVector RayEnd = RayStart + Forward * ObstacleTraceDistance;

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayEnd, ObstacleTraceChannel, Params))
        {
            if (MinDistance < 0.0f || Hit.Distance < MinDistance)
            {
                MinDistance = Hit.Distance;
            }

            if (bDrawDebug)
            {
                DrawDebugLine(GetWorld(), RayStart, Hit.Location,
                    FColor::Red, false, -1.0f, SDPG_World, 2.0f);
            }
        }
        else if (bDrawDebug)
        {
            DrawDebugLine(GetWorld(), RayStart, RayEnd,
                FColor::Green, false, -1.0f, SDPG_World, 2.0f);
        }
    }

    return MinDistance;
}

// ============================================================================
// 前瞻点更新: 速度自适应距离
// ============================================================================

void UVehicleAutopilotComponent::UpdateLookAheadPoint()
{
    if (PathPoints.Num() == 0) return;

    const float Speed = FMath::Abs(GetVehicleSpeed());

    // 前瞻距离 = 速度 × 系数, 钳制在 [Min, Max]
    const float LookAheadDist = FMath::Clamp(
        Speed * LookAheadSpeedFactor,
        MinLookAheadDistance,
        MaxLookAheadDistance);

    // 从当前索引沿路径累积距离, 找到前瞻点
    float AccumulatedDist = 0.0f;

    for (int32 i = CurrentPathIndex; i < PathPoints.Num() - 1; ++i)
    {
        const FVector& Current = PathPoints[i];
        const FVector& Next = PathPoints[i + 1];
        const float SegmentDist = FVector::Dist2D(Current, Next);

        if (AccumulatedDist + SegmentDist >= LookAheadDist)
        {
            // 在这段路径上插值
            const float Remaining = LookAheadDist - AccumulatedDist;
            const float Alpha = Remaining / FMath::Max(SegmentDist, KINDA_SMALL_NUMBER);
            LookAheadPoint = FMath::Lerp(Current, Next, Alpha);
            LookAheadIndex = i + 1;
            return;
        }

        AccumulatedDist += SegmentDist;
    }

    // 到达路径末尾
    LookAheadPoint = PathPoints.Last();
    LookAheadIndex = PathPoints.Num() - 1;
}

// ============================================================================
// 曲率估算: 基于前方路径点夹角
// ============================================================================

float UVehicleAutopilotComponent::EstimateUpcomingCurvature() const
{
    if (CurrentPathIndex >= PathPoints.Num() - 2) return 0.0f;

    const int32 Idx0 = CurrentPathIndex;
    const int32 Idx1 = FMath::Min(Idx0 + 1, PathPoints.Num() - 1);
    const int32 Idx2 = FMath::Min(Idx0 + 2, PathPoints.Num() - 1);

    const FVector V1 = (PathPoints[Idx1] - PathPoints[Idx0]).GetSafeNormal2D();
    const FVector V2 = (PathPoints[Idx2] - PathPoints[Idx1]).GetSafeNormal2D();

    const float Dot = FMath::Clamp(FVector::DotProduct(V1, V2), -1.0f, 1.0f);
    const float Angle = FMath::Acos(Dot); // rad

    const float SegmentLen = FVector::Dist2D(PathPoints[Idx0], PathPoints[Idx1]) +
                             FVector::Dist2D(PathPoints[Idx1], PathPoints[Idx2]);

    if (SegmentLen < KINDA_SMALL_NUMBER) return 0.0f;

    return Angle / SegmentLen; // rad/cm
}

// ============================================================================
// 弯道限速: v = sqrt(a_lat / curvature)
// ============================================================================

float UVehicleAutopilotComponent::ComputeCornerSpeed() const
{
    const float Curvature = EstimateUpcomingCurvature();
    if (Curvature < 0.0001f) return DefaultCruiseSpeed;

    // 物理公式: v_max = sqrt(a_lat_max / k)
    return FMath::Sqrt(MaxLateralAcceleration / Curvature);
}

// ============================================================================
// 状态机: 起步 → 巡航 → 过弯 → 制动 → 到达
// ============================================================================

void UVehicleAutopilotComponent::UpdateStateMachine(float DeltaTime, float ObstacleDistance)
{
    const float Speed = FMath::Abs(GetVehicleSpeed());
    const FVector VehicleLoc = GetVehicleLocation();
    const float DistToEnd = FVector::Dist2D(VehicleLoc, PathPoints.Last());

    // 障碍物检测: 最高优先级
    if (ObstacleDistance > 0.0f && ObstacleDistance < EmergencyBrakeDistance)
    {
        CurrentState = EVehicleAutopilotState::EmergencyStop;
    }

    switch (CurrentState)
    {
    case EVehicleAutopilotState::Idle:
        break;

    case EVehicleAutopilotState::Starting:
        CurrentTargetSpeed = DefaultCruiseSpeed;
        if (Speed > 100.0f) // 起步完成
        {
            CurrentState = EVehicleAutopilotState::Cruising;
        }
        break;

    case EVehicleAutopilotState::Cruising:
    {
        const float CornerSpeed = ComputeCornerSpeed();
        CurrentTargetSpeed = FMath::Min(DefaultCruiseSpeed, CornerSpeed);

        if (DistToEnd < ArrivalDistance)
        {
            CurrentState = EVehicleAutopilotState::Braking;
        }
        else if (CornerSpeed < DefaultCruiseSpeed * 0.7f)
        {
            CurrentState = EVehicleAutopilotState::Cornering;
        }
        break;
    }

    case EVehicleAutopilotState::Cornering:
    {
        const float CornerSpeed = ComputeCornerSpeed();
        CurrentTargetSpeed = FMath::Min(DefaultCruiseSpeed, CornerSpeed);

        if (DistToEnd < ArrivalDistance)
        {
            CurrentState = EVehicleAutopilotState::Braking;
        }
        else if (CornerSpeed >= DefaultCruiseSpeed * 0.7f)
        {
            CurrentState = EVehicleAutopilotState::Cruising;
        }
        break;
    }

    case EVehicleAutopilotState::Braking:
    {
        // 距终点越近, 目标速度越低
        const float SpeedRatio = FMath::Clamp(DistToEnd / ArrivalDistance, 0.0f, 1.0f);
        CurrentTargetSpeed = DefaultCruiseSpeed * SpeedRatio;

        if (DistToEnd < StopDistance || Speed < StopSpeedThreshold)
        {
            CurrentState = EVehicleAutopilotState::Arrived;
        }
        break;
    }

    case EVehicleAutopilotState::Arrived:
    {
        CurrentTargetSpeed = 0.0f;
        if (Speed < StopSpeedThreshold)
        {
            if (CachedVehicleMovement)
            {
                CachedVehicleMovement->SetThrottleInput(0.0f);
                CachedVehicleMovement->SetBrakeInput(1.0f);
            }
            OnArrived.Broadcast();
            StopAutopilot();
        }
        break;
    }

    case EVehicleAutopilotState::EmergencyStop:
    {
        CurrentTargetSpeed = 0.0f;
        // 障碍清除后恢复行驶
        if (ObstacleDistance < 0.0f || ObstacleDistance > ObstacleTraceDistance)
        {
            CurrentState = EVehicleAutopilotState::Cruising;
        }
        break;
    }
    }

    // 非紧急情况下, 障碍物在检测范围内时按距离比例减速
    if (ObstacleDistance > 0.0f &&
        ObstacleDistance >= EmergencyBrakeDistance &&
        ObstacleDistance < ObstacleTraceDistance &&
        CurrentState != EVehicleAutopilotState::EmergencyStop &&
        CurrentState != EVehicleAutopilotState::Arrived)
    {
        const float SlowRatio = ObstacleDistance / ObstacleTraceDistance;
        CurrentTargetSpeed *= SlowRatio;
    }
}

// ============================================================================
// 路径索引查找: 只向前搜索, 防止回头
// ============================================================================

int32 UVehicleAutopilotComponent::FindClosestPathIndex() const
{
    if (PathPoints.Num() == 0) return 0;

    const FVector VehicleLoc = GetVehicleLocation();
    int32 ClosestIndex = CurrentPathIndex;
    float ClosestDist = FVector::DistSquared2D(VehicleLoc, PathPoints[CurrentPathIndex]);

    // 向前搜索, 限制窗口大小
    const int32 SearchLimit = FMath::Min(CurrentPathIndex + 10, PathPoints.Num());
    for (int32 i = CurrentPathIndex + 1; i < SearchLimit; ++i)
    {
        const float Dist = FVector::DistSquared2D(VehicleLoc, PathPoints[i]);
        if (Dist < ClosestDist)
        {
            ClosestDist = Dist;
            ClosestIndex = i;
        }
    }

    return ClosestIndex;
}

// ============================================================================
// 辅助函数
// ============================================================================

AWheeledVehiclePawnBase* UVehicleAutopilotComponent::GetVehicleOwner() const
{
    return Cast<AWheeledVehiclePawnBase>(GetOwner());
}

FVector UVehicleAutopilotComponent::GetVehicleLocation() const
{
    if (AWheeledVehiclePawnBase* Vehicle = GetVehicleOwner())
    {
        return Vehicle->GetActorLocation();
    }
    return FVector::ZeroVector;
}

FVector UVehicleAutopilotComponent::GetVehicleForward() const
{
    if (AWheeledVehiclePawnBase* Vehicle = GetVehicleOwner())
    {
        return Vehicle->GetActorForwardVector();
    }
    return FVector::ForwardVector;
}

float UVehicleAutopilotComponent::GetVehicleSpeed() const
{
    if (!CachedVehicleMovement) return 0.0f;
    return CachedVehicleMovement->GetForwardSpeed(); // cm/s, 正=前进
}
