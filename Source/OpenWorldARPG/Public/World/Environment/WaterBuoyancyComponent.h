// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaterBuoyancyComponent.generated.h"

/**
 * 浮力组件
 *
 * 附加到任何带物理模拟的 Actor 上，使其在水中浮起。
 * 工作原理：
 * - 在多个浮力采样点向下检测水面高度
 * - 根据浸没深度施加向上的浮力
 * - 施加水阻尼（线性+角速度）模拟水的粘性
 * - 支持水流推力（从 AWaterVolume 读取）
 *
 * 典型用途：
 * - 木箱/桶浮在水面
 * - 掉落物在水中漂浮
 * - 简易物理浮台
 */
UCLASS(ClassGroup = (Environment), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UWaterBuoyancyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWaterBuoyancyComponent();

    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    // --- 浮力配置 ---

    /** 浮力强度（力 / 浸没深度，值越大浮得越快） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    float BuoyancyStrength = 5000.0f;

    /** 最大浮力（防止过力弹射） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    float MaxBuoyancyForce = 20000.0f;

    /** 水的线性阻尼（值越大减速越快） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    float WaterLinearDamping = 1.5f;

    /** 水的角速度阻尼（值越大旋转减速越快） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    float WaterAngularDamping = 2.0f;

    /** 浮力采样点（本地坐标偏移，建议 4~8 个点分布在物体底部） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    TArray<FVector> BuoyancyPoints;

    /** 水面检测通道 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buoyancy")
    TEnumAsByte<ECollisionChannel> WaterTraceChannel = ECollisionChannel::ECC_GameTraceChannel1;

private:
    /** 查找重叠的水体 Volume */
    class AWaterVolume* FindOverlappingWaterVolume() const;

    /** 初始化默认浮力采样点 */
    void InitDefaultBuoyancyPoints();
};
