// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "WaterMovementComponent.generated.h"

/**
 * 水面载具运动组件（轮船/航母物理模型）
 *
 * 物理模型：
 * - 浮力：向下射线检测水面，弹簧阻尼贴合水面高度
 * - 波浪响应：船头/船尾/左舷/右舷四点采样水面高度，计算俯仰和横滚
 * - 推进：油门控制前进速度，大惯性加速/减速
 * - 舵转向：需要速度才能转向（速度=0时不转），转向速率与速度成正比
 * - 水阻：松油门时自然减速，无主动刹车
 *
 * 视觉架构：
 * - Root (碰撞体) 保持竖直，Z 贴合水面
 * - TiltPivot (网格体挂点) 承载波浪俯仰/横滚
 * - 与 HoverVehiclePawnBase 的 TiltPivot 模式一致
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UWaterMovementComponent : public UPawnMovementComponent
{
    GENERATED_BODY()

public:
    UWaterMovementComponent();

    // --- 输入接口 ---

    /** 油门输入 (-1.0 ~ 1.0, 正=前进, 负=倒退) */
    UFUNCTION(BlueprintCallable, Category = "Water|Input")
    void SetThrottleInput(float Value) { ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 舵输入 (-1.0 ~ 1.0, 正=右转, 负=左转) */
    UFUNCTION(BlueprintCallable, Category = "Water|Input")
    void SetRudderInput(float Value) { RudderInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 清零所有输入 */
    UFUNCTION(BlueprintCallable, Category = "Water|Input")
    void ClearInput();

    // --- 状态查询 ---

    /** 当前前进速度 (cm/s) */
    UFUNCTION(BlueprintCallable, Category = "Water|State", BlueprintPure)
    float GetCurrentSpeed() const { return CurrentSpeed; }

    /** 当前速度 / 最大速度 (0~1) */
    UFUNCTION(BlueprintCallable, Category = "Water|State", BlueprintPure)
    float GetSpeedRatio() const;

    /** 是否在水中 */
    UFUNCTION(BlueprintCallable, Category = "Water|State", BlueprintPure)
    bool IsInWater() const { return bIsInWater; }

    // --- 视觉倾斜数据 (供 Pawn 应用到 TiltPivot) ---

    /** 波浪导致的俯仰角 (度, 正=船头上扬) */
    UFUNCTION(BlueprintCallable, Category = "Water|Visual", BlueprintPure)
    float GetWavePitch() const { return WavePitch; }

    /** 波浪导致的横滚角 (度, 正=右舷下沉) */
    UFUNCTION(BlueprintCallable, Category = "Water|Visual", BlueprintPure)
    float GetWaveRoll() const { return WaveRoll; }

    // --- 生命周期 ---

    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    // --- 推进配置 ---

    /** 最大前进速度 (cm/s, 1500 ≈ 54 km/h 巡航) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Propulsion")
    float MaxForwardSpeed = 1500.0f;

    /** 最大倒退速度 (cm/s, 通常为前进的 1/3) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Propulsion")
    float MaxReverseSpeed = 500.0f;

    /** 加速度 (cm/s², 船舶加速慢) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Propulsion")
    float Acceleration = 200.0f;

    /** 水阻减速 (cm/s², 松油门自然减速) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Propulsion")
    float WaterDrag = 100.0f;

    // --- 转向配置 ---

    /** 最大舵转向速率 (度/秒, 满速时) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Steering")
    float MaxTurnRate = 20.0f;

    /** 最小转向速度 (cm/s, 低于此速度不转向) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Steering")
    float MinTurnSpeed = 100.0f;

    // --- 浮力配置 ---

    /** 吃水深度 (cm, 船体下沉到水面的距离) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Buoyancy")
    float Draft = 50.0f;

    /** 浮力刚度 (值越大贴水越快, 过大会抖) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Buoyancy")
    float BuoyancyStiffness = 5.0f;

    /** 浮力阻尼 (抑制水面弹跳) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Buoyancy")
    float BuoyancyDamping = 3.0f;

    /** 水面检测通道 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Buoyancy")
    TEnumAsByte<ECollisionChannel> WaterTraceChannel = ECC_WorldStatic;

    /** 默认水面高度 Z (cm, 射线未命中时回退使用) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Buoyancy")
    float DefaultWaterLevel = 0.0f;

    // --- 波浪响应配置 ---

    /** 启用波浪响应（四点采样俯仰/横滚） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Wave")
    bool bEnableWaveResponse = true;

    /** 船体长度 (cm, 用于波浪采样间距) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Wave")
    float HullLength = 800.0f;

    /** 船体宽度 (cm, 用于波浪采样间距) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Wave")
    float HullWidth = 300.0f;

    /** 波浪采样间隔 (秒, 降低频率以优化性能) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Wave")
    float WaveSampleInterval = 0.05f;

    /** 波浪倾斜平滑速度 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water|Wave")
    float WaveTiltSmoothSpeed = 5.0f;

private:
    // --- 输入 ---
    float ThrottleInput = 0.0f;
    float RudderInput = 0.0f;

    // --- 运行时状态 ---
    float CurrentSpeed = 0.0f;
    bool bIsInWater = false;
    float CurrentVerticalVelocity = 0.0f;

    // --- 波浪采样 ---
    float WaveSampleTimer = 0.0f;
    float WavePitch = 0.0f;
    float WaveRoll = 0.0f;

    // --- 内部方法 ---

    /** 采样指定位置的水面高度 */
    float SampleWaterSurface(FVector WorldLocation) const;

    /** 四点采样波浪俯仰/横滚 */
    void UpdateWaveResponse(float DeltaTime);

    /** 浮力更新：贴合水面高度 */
    void UpdateBuoyancy(float DeltaTime, float TargetWaterZ);
};
