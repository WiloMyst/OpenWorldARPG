// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "ClimbingComponent.generated.h"

class APlayerCharacter;
class UCharacterMovementComponent;
class UOpenWorldARPGCharacterMovementComponent;
class UAbilitySystemComponent;
class UAnimMontage;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UClimbingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UClimbingComponent();

    // ==========================================
    // 核心接口
    // ==========================================

    /** 尝试攀爬 (检测墙壁 + 切换模式) */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void TryClimb();

    /** 退出攀爬模式 */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void ExitClimb();

    /** 检测头部上方是否有障碍物并尝试翻越 */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void CheckAndClimbUp();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOwnerMovementInput(float InputX, float InputY);

    /** 进入攀爬模式 */
    void EnterClimb(const FHitResult& WallHit);

    /** 翻越动作 */
    void DoClimbUp();

    // 翻越位移结束后的回调函数
    UFUNCTION()
    void OnClimbUpFinished();

    /** 执行胸部与头部射线检测 (支持偏移检测) */
    bool PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit);

protected:
    // ==========================================
    // 配置项：射线检测
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceDistance = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    FName HeadSocketName = FName("Head");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbUpCheckRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbUpCheckHalfHeight = 50.0f;

    // ==========================================
    // 配置项：逻辑判断与标签
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Logic")
    float MinInputDotProduct = -0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FName UnclimbableActorTag = FName("NotClimbable");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTagContainer BlockClimbingTags;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ClimbingStateTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag EventClimbStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag EventClimbStopTag;

    // ==========================================
    // 配置项：攀爬检测 (移动参数已迁移到 CMC)
    // ==========================================

    /** 攀爬预测偏移量乘数 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float ClimbPredictOffset = 50.0f;

    /** 攀爬周围碰撞检测胶囊体半径 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckRadius = 20.0f;

    /** 攀爬周围碰撞检测胶囊体半高 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckHalfHeight = 90.0f;

    // ==========================================
    // 配置项：表现与动画
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapZOffset = -65.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapTime = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> ClimbUpMontage;

    // 翻越时，胶囊体需要向前和向上移动的偏移量
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    FVector ClimbUpOffset = FVector(40.0f, 0.0f, 110.0f);

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> MovementComp;

    UPROPERTY()
    TObjectPtr<UOpenWorldARPGCharacterMovementComponent> CustomMovementComp;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC;

    // 状态锁：是否正在执行翻越动作
    bool bIsClimbingUp = false;
};
