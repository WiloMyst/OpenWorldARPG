// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Components/VehicleMovementComponent_Predictive.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"

// ============================================================================
// TickComponent — 三端分流：服务端权威 / 本地预测 / 远端外推
// ============================================================================

void UVehicleMovementComponent_Predictive::TickComponent(
    float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!PawnOwner || !UpdatedComponent) return;

    if (PawnOwner->HasAuthority())
    {
        // 服务端：权威模拟
        SimulateMovement(DeltaTime);
    }
    else if (PawnOwner->IsLocallyControlled())
    {
        // 本地客户端：预测模拟 + 纠错
        SimulateMovement(DeltaTime);
        if (bHasPendingCorrection)
        {
            ApplyCorrection(DeltaTime);
        }
    }
    else
    {
        // 远端客户端：dead reckoning 外推
        ExtrapolateRemote(DeltaTime);
    }
}

// ============================================================================
// OnServerStateReceived — 由 Pawn::OnRep_ReplicatedMovement 调用
// ============================================================================

void UVehicleMovementComponent_Predictive::OnServerStateReceived(const FRepMovement& ServerState)
{
    if (!PawnOwner || !UpdatedComponent) return;

    // 远端：更新外推速度
    LastKnownVelocity = ServerState.LinearVelocity;

    // 仅本地控制端做预测纠错
    if (!PawnOwner->IsLocallyControlled() || PawnOwner->HasAuthority()) return;

    const FVector CurrentLoc = UpdatedComponent->GetComponentLocation();
    const float Error = FVector::Dist(ServerState.Location, CurrentLoc);

    if (Error > SnapThreshold)
    {
        // 大误差：硬传送，避免长时间拉回
        UpdatedComponent->SetWorldLocationAndRotation(
            ServerState.Location, ServerState.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
        bHasPendingCorrection = false;
    }
    else if (Error > CorrectionThreshold)
    {
        // 中等误差：启动平滑插值纠错
        bHasPendingCorrection = true;
        CorrectionStartLocation = CurrentLoc;
        CorrectionStartRotation = UpdatedComponent->GetComponentQuat();
        CorrectionTargetLocation = ServerState.Location;
        CorrectionTargetRotation = ServerState.Rotation.Quaternion();
        CorrectionAlpha = 0.0f;
    }
    // 小误差：忽略，继续本地预测
}

// ============================================================================
// ApplyCorrection — 平滑插值纠错
// ============================================================================

void UVehicleMovementComponent_Predictive::ApplyCorrection(float DeltaTime)
{
    if (!UpdatedComponent) return;

    CorrectionAlpha += DeltaTime / FMath::Max(CorrectionDuration, KINDA_SMALL_NUMBER);
    const float Alpha = FMath::Clamp(CorrectionAlpha, 0.0f, 1.0f);

    // SmoothStep 插值：起止平滑，中段加速
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

    const FVector NewLoc = FMath::Lerp(CorrectionStartLocation, CorrectionTargetLocation, SmoothAlpha);
    const FQuat NewRot = FQuat::Slerp(CorrectionStartRotation, CorrectionTargetRotation, SmoothAlpha);

    UpdatedComponent->SetWorldLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::None);

    if (Alpha >= 1.0f)
    {
        bHasPendingCorrection = false;
    }
}

// ============================================================================
// ExtrapolateRemote — 远端 dead reckoning 外推
// ============================================================================

void UVehicleMovementComponent_Predictive::ExtrapolateRemote(float DeltaTime)
{
    if (!UpdatedComponent) return;

    const FVector MoveDelta = LastKnownVelocity * DeltaTime;
    if (!MoveDelta.IsNearlyZero())
    {
        FHitResult Hit;
        SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
    }
}
