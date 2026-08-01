// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Components/VehicleMovementComponent_Predictive.h"
#include "HoverMovementComponent.generated.h"

/**
 * 悬停载具运动组件（四旋翼/无人机物理模型）
 *
 * 物理模型：
 * - 全向移动：前后/左右/上下三轴独立控制，可横移
 * - 悬停保持：无油门输入时自动抵消重力，保持高度
 * - 偏航旋转：独立旋转，不影响移动方向
 * - 视觉倾斜：移动时机身向运动方向倾斜（纯视觉，不改变移动向量）
 * - 精确模式：降速用于着陆/拾取
 *
 * 与 UFlightMovementComponent 的区别：
 * - 无失速、无推力-速度曲线，直接控制三轴速度
 * - 无银行转弯，偏航和横移独立
 * - 支持原地悬停
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UHoverMovementComponent : public UVehicleMovementComponent_Predictive
{
    GENERATED_BODY()

public:
    UHoverMovementComponent();

    // --- 输入接口 ---

    /** 前后输入 (-1.0 ~ 1.0, 正=前进) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void SetForwardInput(float Value) { ForwardInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 横移输入 (-1.0 ~ 1.0, 正=右移) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void SetStrafeInput(float Value) { StrafeInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 油门输入 (-1.0 ~ 1.0, 正=上升, 负=下降) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void SetThrottleInput(float Value) { ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 偏航输入 (-1.0 ~ 1.0, 正=右转) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void SetYawInput(float Value) { YawInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 精确模式 (降速用于着陆/拾取) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void SetPrecisionMode(bool bEnabled) { bIsPrecision = bEnabled; }

    /** 清零所有输入 */
    UFUNCTION(BlueprintCallable, Category = "Hover|Input")
    void ClearInput();

    // --- 输入状态查询（供 Pawn 打包发送 Server RPC）---

    float GetForwardInput() const { return ForwardInput; }
    float GetStrafeInput() const { return StrafeInput; }
    float GetThrottleInput() const { return ThrottleInput; }
    float GetYawInput() const { return YawInput; }
    bool IsPrecisionMode() const { return bIsPrecision; }

    // --- 状态查询 (供 UI/动画读取) ---

    /** 当前水平速度 (cm/s) */
    UFUNCTION(BlueprintCallable, Category = "Hover|State", BlueprintPure)
    float GetCurrentHorizontalSpeed() const { return CurrentHorizontalVelocity.Size2D(); }

    /** 当前垂直速度 (cm/s, 正=上升) */
    UFUNCTION(BlueprintCallable, Category = "Hover|State", BlueprintPure)
    float GetCurrentVerticalSpeed() const { return CurrentVerticalVelocity; }

    /** 当前高度 (cm) */
    UFUNCTION(BlueprintCallable, Category = "Hover|State", BlueprintPure)
    float GetCurrentAltitude() const;

    /** 是否在悬停（水平+垂直速度都接近 0） */
    UFUNCTION(BlueprintCallable, Category = "Hover|State", BlueprintPure)
    bool IsHovering() const;

    // --- 视觉倾斜数据 (供 Pawn 应用到 Mesh) ---

    /** 机身俯仰倾斜角度 (度, 正=机鼻下压) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Visual", BlueprintPure)
    float GetTiltPitch() const { return TiltPitch; }

    /** 机身横滚倾斜角度 (度, 正=右倾) */
    UFUNCTION(BlueprintCallable, Category = "Hover|Visual", BlueprintPure)
    float GetTiltRoll() const { return TiltRoll; }

    // --- 生命周期 ---

    virtual void SimulateMovement(float DeltaTime) override;

protected:
    // --- 速度配置 ---

    /** 最大水平速度 (cm/s, 1000 ≈ 36 km/h) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Speed")
    float MaxHorizontalSpeed = 1000.0f;

    /** 最大垂直速度 (cm/s) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Speed")
    float MaxVerticalSpeed = 600.0f;

    /** 精确模式速度倍率 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Speed")
    float PrecisionSpeedMultiplier = 0.3f;

    /** 加速度 (cm/s²) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Speed")
    float Acceleration = 800.0f;

    /** 减速度 (cm/s², 松手时) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Speed")
    float Deceleration = 1200.0f;

    // --- 偏航配置 ---

    /** 偏航速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Rotation")
    float YawRate = 90.0f;

    // --- 视觉倾斜配置 ---

    /** 最大倾斜角度 (度) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Tilt")
    float MaxTiltAngle = 20.0f;

    /** 倾斜恢复速度 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Tilt")
    float TiltRecoverySpeed = 8.0f;

    // --- 悬停配置 ---

    /** 悬停死区（油门输入绝对值低于此值时视为悬停） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Hover")
    float HoverDeadzone = 0.1f;

    /** 悬停时重力补偿速度 (cm/s², 补偿越快越稳) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover|Hover")
    float HoverGravityCompensation = 2000.0f;

private:
    // --- 输入状态 ---
    float ForwardInput = 0.0f;
    float StrafeInput = 0.0f;
    float ThrottleInput = 0.0f;
    float YawInput = 0.0f;
    bool bIsPrecision = false;

    // --- 速度状态 ---
    FVector CurrentHorizontalVelocity = FVector::ZeroVector;
    float CurrentVerticalVelocity = 0.0f;

    // --- 视觉倾斜 ---
    float TiltPitch = 0.0f;
    float TiltRoll = 0.0f;
};
