// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "VehicleMovementComponent_Predictive.generated.h"

/**
 * 运动学载具移动组件的客户端预测基类。
 *
 * 【设计理念】
 * Hover/Flight 是运动学移动组件（非 Chaos 物理），无法使用 UNetworkPhysicsComponent。
 * 本基类提供简化预测框架：客户端即时模拟 + 服务端权威 + 平滑纠错（无回滚重放）。
 *
 * 【三端 Tick 分流】
 * - 服务端（HasAuthority）：权威模拟 SimulateMovement
 * - 本地客户端（IsLocallyControlled）：预测模拟 SimulateMovement + 纠错 ApplyCorrection
 * - 远端客户端（SimulatedProxy）：用最后已知速度做 dead reckoning 外推
 *
 * 【纠错机制】
 * Pawn 的 OnRep_ReplicatedMovement 调用 OnServerStateReceived，
 * 比较本地预测位置与服务端权威位置：
 * - 误差 < CorrectionThreshold：忽略，继续本地预测
 * - 误差 ∈ [CorrectionThreshold, SnapThreshold]：启动平滑插值纠错
 * - 误差 > SnapThreshold：硬传送（TeleportPhysics），避免长时间拉回
 *
 * 【子类职责】
 * 实现 SimulateMovement(DeltaTime) — 纯运动学模拟逻辑，不含网络判断。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UVehicleMovementComponent_Predictive : public UPawnMovementComponent
{
    GENERATED_BODY()

public:
    /** 子类实现：纯运动学模拟（由基类 TickComponent 在正确上下文中调用） */
    virtual void SimulateMovement(float DeltaTime) {}

    /**
     * 由 Pawn 的 OnRep_ReplicatedMovement 调用。
     * 本地控制端：检测预测误差，启动纠错。
     * 远端：更新外推速度。
     */
    void OnServerStateReceived(const FRepMovement& ServerState);

    // --- 预测纠错配置 ---

    /** 误差低于此值不纠错（cm），容忍轻微预测偏差 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network|Prediction")
    float CorrectionThreshold = 50.0f;

    /** 误差高于此值直接硬传送（cm），避免长时间拉回 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network|Prediction")
    float SnapThreshold = 300.0f;

    /** 平滑纠错时长（秒），误差在此时间内插值完成 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network|Prediction")
    float CorrectionDuration = 0.15f;

protected:
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // --- 预测纠错状态 ---

    bool bHasPendingCorrection = false;
    FVector CorrectionStartLocation = FVector::ZeroVector;
    FQuat CorrectionStartRotation = FQuat::Identity;
    FVector CorrectionTargetLocation = FVector::ZeroVector;
    FQuat CorrectionTargetRotation = FQuat::Identity;
    float CorrectionAlpha = 0.0f;

    /** 平滑插值纠错（在 CorrectionDuration 内从 Start 到 Target） */
    void ApplyCorrection(float DeltaTime);

    // --- 远端外推状态 ---

    FVector LastKnownVelocity = FVector::ZeroVector;

    /** 远端 dead reckoning：用最后已知速度线性外推 */
    void ExtrapolateRemote(float DeltaTime);
};
