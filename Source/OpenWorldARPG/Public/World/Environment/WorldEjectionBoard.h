// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WorldEjectionBoard.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 大世界环境交互物：弹射跳板
 * 职责：在角色踩踏时，打断指定状态并给予强力弹射。
 * 优化：数据驱动，彻底解耦，不依赖特定角色类。
 */
UCLASS()
class OPENWORLDARPG_API AWorldEjectionBoard : public AActor
{
    GENERATED_BODY()

public:
    AWorldEjectionBoard();

protected:
    virtual void BeginPlay() override;

    // ==========================================
    // 组件声明
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BoardMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // ==========================================
    // 配置参数 (将蓝图硬编码暴露给策划)
    // ==========================================
    
    /** 弹射速度向量 (默认向上 3000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EjectionBoard | Launch Settings")
    FVector LaunchVelocity;

    /** 是否强制覆盖角色当前的水平 (X/Y) 速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EjectionBoard | Launch Settings")
    bool bXYOverride;

    /** 是否强制覆盖角色当前的垂直 (Z) 速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EjectionBoard | Launch Settings")
    bool bZOverride;

    /** 弹射前需要强制打断的技能 Tags (如：瞄准、蓄力攻击、空中状态等) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EjectionBoard | Launch Settings")
    FGameplayTagContainer TagsToCancelOnLaunch;

private:
    // 碰撞触发回调
    UFUNCTION()
    void OnTriggerBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};