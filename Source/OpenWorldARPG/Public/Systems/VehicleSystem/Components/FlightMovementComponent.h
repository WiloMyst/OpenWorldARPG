// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Components/VehicleMovementComponent_Predictive.h"
#include "FlightMovementComponent.generated.h"

/**
 * 飞行载具运动组件（街机物理模型）
 *
 * 物理模型：
 * - 推力：油门控制前进速度，带平滑加速/减速
 * - 旋转：俯仰/偏航/横滚三轴独立控制，本地空间旋转
 * - 升力：速度 > 失速速度时自动抵消重力
 * - 失速：速度 < 失速速度时机鼻自动下俯 + 重力下坠
 * - 银行转弯：偏航输入时自动倾斜（可配置角度）
 * - 加速：临时提升最大速度（Shift 键）
 *
 * 网络规则：
 * - 客户端即时模拟（预测），通过 Pawn 的 Server RPC 发送输入
 * - 服务端权威模拟，通过 ReplicatedMovement 纠正客户端预测
 * - 远端客户端用 dead reckoning 外推
 * - 预测纠错由 UVehicleMovementComponent_Predictive 基类处理
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UFlightMovementComponent : public UVehicleMovementComponent_Predictive
{
    GENERATED_BODY()

public:
    UFlightMovementComponent();

    // --- 输入接口 ---

    /** 油门输入 (-1.0 ~ 1.0, 正=前进, 负=减速/倒退) */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void SetThrottleInput(float Value) { ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 俯仰输入 (-1.0 ~ 1.0, 正=拉杆/机鼻上仰) */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void SetPitchInput(float Value) { PitchInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 横滚输入 (-1.0 ~ 1.0, 正=右滚) */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void SetRollInput(float Value) { RollInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 偏航输入 (-1.0 ~ 1.0, 正=右偏) */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void SetYawInput(float Value) { YawInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 加速输入 (true=开启加速) */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void SetBoostInput(bool bEnabled) { bIsBoosting = bEnabled; }

    /** 清零所有输入 */
    UFUNCTION(BlueprintCallable, Category = "Flight|Input")
    void ClearInput();

    // --- 输入状态查询（供 Pawn 打包发送 Server RPC）---

    float GetThrottleInput() const { return ThrottleInput; }
    float GetPitchInput() const { return PitchInput; }
    float GetRollInput() const { return RollInput; }
    float GetYawInput() const { return YawInput; }

    // --- 状态查询 (供 UI/HUD 读取) ---

    /** 当前前进速度 (cm/s) */
    UFUNCTION(BlueprintCallable, Category = "Flight|State", BlueprintPure)
    float GetCurrentSpeed() const { return CurrentSpeed; }

    /** 当前速度 / 最大速度 (0~1, 供 UI 速度条) */
    UFUNCTION(BlueprintCallable, Category = "Flight|State", BlueprintPure)
    float GetSpeedRatio() const;

    /** 是否正在失速 */
    UFUNCTION(BlueprintCallable, Category = "Flight|State", BlueprintPure)
    bool IsStalling() const { return bIsStalling; }

    /** 是否正在加速 */
    UFUNCTION(BlueprintCallable, Category = "Flight|State", BlueprintPure)
    bool IsBoosting() const { return bIsBoosting; }

    // --- 生命周期 ---

    virtual void SimulateMovement(float DeltaTime) override;

protected:
    // --- 推力配置 ---

    /** 最大巡航速度 (cm/s, 8000 ≈ 288 km/h) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Thrust")
    float MaxCruiseSpeed = 8000.0f;

    /** 加速时最大速度 (cm/s) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Thrust")
    float MaxBoostSpeed = 14000.0f;

    /** 加速度 (cm/s²) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Thrust")
    float Acceleration = 1500.0f;

    /** 减速度 (cm/s², 松油门时的自然减速) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Thrust")
    float Deceleration = 500.0f;

    // --- 旋转配置 ---

    /** 俯仰速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Rotation")
    float PitchRate = 60.0f;

    /** 偏航速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Rotation")
    float YawRate = 40.0f;

    /** 横滚速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Rotation")
    float RollRate = 90.0f;

    /** 银行转弯最大倾斜角度 (度, 偏航时自动横滚) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Rotation")
    float AutoBankMaxAngle = 35.0f;

    /** 银行转弯回正速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Rotation")
    float AutoBankRecoveryRate = 45.0f;

    // --- 升力/失速配置 ---

    /** 失速速度 (cm/s, 低于此速度开始下坠) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Stall")
    float StallSpeed = 1500.0f;

    /** 失速时机鼻自动下俯速率 (度/秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Stall")
    float StallAutoPitchRate = 15.0f;

    /** 重力加速度 (cm/s²) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Stall")
    float Gravity = 980.0f;

    // --- 输入平滑 ---

    /** 输入平滑速度 (值越大响应越快) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Input")
    float InputSmoothSpeed = 8.0f;

private:
    // --- 输入状态 ---
    float ThrottleInput = 0.0f;
    float PitchInput = 0.0f;
    float RollInput = 0.0f;
    float YawInput = 0.0f;
    bool bIsBoosting = false;

    // --- 平滑后的输入 ---
    float SmoothedPitch = 0.0f;
    float SmoothedYaw = 0.0f;
    float SmoothedRoll = 0.0f;

    // --- 运行时状态 ---
    float CurrentSpeed = 0.0f;
    bool bIsStalling = false;
    float CurrentBankAngle = 0.0f;
};
