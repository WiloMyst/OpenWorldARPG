// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterVolume.generated.h"

class UBoxComponent;

// --- 委托声明 ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorEnterWater, AActor*, EnteringActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorExitWater, AActor*, ExitingActor);

/**
 * 水体区域定义 Actor
 *
 * 职责：
 * - 在场景中定义一个可进入的水体区域（Box 碰撞体）
 * - 碰撞响应 WaterTraceChannel（Overlap），供 CMC 的 IsStillInWater() 检测
 * - 碰撞响应 Pawn 通道（Overlap），供 GetOverlappingActors() 查询
 * - 提供水面 Z 高度（Box 顶部）
 * - 提供水流方向和强度（河流/洋流）
 * - 广播进水/出水事件
 *
 * 视觉层不由此类负责：
 * - 水面渲染用 UE5 Water Plugin 或自制 Water Material
 * - 水下后处理用 PostProcessVolume
 * - 此类仅负责 Game Logic 层
 */
UCLASS(Blueprintable, ClassGroup = (Environment))
class OPENWORLDARPG_API AWaterVolume : public AActor
{
    GENERATED_BODY()

public:
    AWaterVolume();

    // --- 水面查询 ---

    /** 获取水面 Z 高度（Box 顶部） */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Water")
    float GetWaterSurfaceZ() const;

    /** 获取水体深度（Box 底部到顶部） */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Water")
    float GetWaterDepth() const;

    /** 获取水流力向量（方向 × 强度） */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Water|Current")
    FVector GetCurrentForce() const;

    // --- 事件委托 ---

    /** Actor 进入水中 */
    UPROPERTY(BlueprintAssignable, Category = "Water|Events")
    FOnActorEnterWater OnActorEnterWater;

    /** Actor 离开水中 */
    UPROPERTY(BlueprintAssignable, Category = "Water|Events")
    FOnActorExitWater OnActorExitWater;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleWaterBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleWaterEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
    // --- 配置 ---

    /** 水体碰撞体（定义水域范围） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water|Components")
    TObjectPtr<UBoxComponent> WaterCollision;

    /** 水流方向（归一化向量，0=无水流） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Current", meta = (MakeEditWidget = true))
    FVector CurrentDirection = FVector::ZeroVector;

    /** 水流强度 (cm/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Current", meta = (ClampMin = "0.0"))
    float CurrentStrength = 0.0f;

    /** 水温（度，可用于后续生存系统） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water|Properties", meta = (ClampMin = "-50.0", ClampMax = "100.0"))
    float WaterTemperature = 15.0f;

    /** 是否允许潜水（false=角色只能浮在水面） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water|Properties")
    bool bAllowDiving = true;
};
