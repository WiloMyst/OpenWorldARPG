// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WorldAccelerationBoard.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UArrowComponent;

/**
 * 大世界环境交互物：定向加速板
 * 职责：根据预设的箭头方向，给予踩踏角色强大的水平推力与轻微的浮空力。
 * 优化：脱离蓝图硬编码数学节点，箭头方向驱动，全参数数据化。
 */
UCLASS()
class OPENWORLDARPG_API AWorldAccelerationBoard : public AActor
{
    GENERATED_BODY()

public:
    AWorldAccelerationBoard();

protected:
    virtual void BeginPlay() override;

    // ==========================================
    // 组件声明
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    /** 用于在编辑器中直观指示加速方向的箭头 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* DirectionArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BoardMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // ==========================================
    // 加速配置参数 (替换蓝图中的硬编码)
    // ==========================================
    
    /** 沿箭头前向的加速力度 (对应你蓝图里的"弹射速度"变量，默认 1000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AccelerationBoard | Acceleration Settings")
    float ForwardAccelerationSpeed;

    /** 垂直方向的浮空补偿力 (对应你蓝图数学节点写死的 Z=1000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AccelerationBoard | Acceleration Settings")
    float UpwardBoostSpeed;

    /** 是否强制覆盖角色当前的水平 (X/Y) 速度 (加速带通常为 True) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AccelerationBoard | Acceleration Settings")
    bool bXYOverride;

    /** 是否强制覆盖角色当前的垂直 (Z) 速度 (通常为 False，保留角色原有的重力/跳跃惯性) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AccelerationBoard | Acceleration Settings")
    bool bZOverride;

    /** 加速前需要强制打断的技能 Tags (如：空中停留、瞄准等) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AccelerationBoard | Acceleration Settings")
    FGameplayTagContainer TagsToCancelOnLaunch;

private:
    // 碰撞触发回调
    UFUNCTION()
    void OnTriggerBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};