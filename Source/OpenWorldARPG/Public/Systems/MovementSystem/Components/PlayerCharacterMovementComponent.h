// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/MovementSystem/Components/BaseCharacterMovementComponent.h"
#include "Systems/MovementSystem/Types/CustomMovementModeTypes.h"
#include "Systems/MovementSystem/Types/MovementStateTypes.h"
#include "PlayerCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClimbUpMontageRequested, UAnimMontage*, MontageToPlay);

/**
 * 玩家角色移动组件，继承自 UBaseCharacterMovementComponent。
 * 管理玩家专属的自定义移动模式：攀爬、滑翔、游泳、冲刺、慢走、瞄准。
 * 基类负责通用状态 Tag 管理与基础物理恢复，本类负责玩家专属物理模拟与状态检测。
 */
UCLASS()
class OPENWORLDARPG_API UPlayerCharacterMovementComponent : public UBaseCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UPlayerCharacterMovementComponent();

    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

    // --- 攀爬接口 ---

    void TryClimb();
    void EnterClimb(const FHitResult& WallHit);
    void ExitClimb();
    void TryClimbUp();
    void FinishClimbUp();
    void DoWallEject();
    bool DetectClimbableWall(FHitResult& OutChestHit);
    FTransform CalculateClimbWarpTarget(const FHitResult& WallHit) const;

    bool IsClimbing() const;
    bool IsClimbingUp() const { return bIsClimbingUp; }
    bool IsInCornerTransition() const;
    FVector GetClimbWallNormal() const { return ClimbWallNormal; }

    // --- 滑翔接口 ---

    void EnterGlideMode();
    void ExitGlideMode();
    bool IsGliding() const { return MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding; }
    float GetDistanceToGround() const;

    // --- 游泳接口 ---

    void EnterSwimMode();
    void ExitSwimMode();
    void EnterFastSwimMode();
    void ExitFastSwimMode();
    bool IsSwimming() const { return MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Swimming; }
    bool IsFastSwimming() const { return bIsFastSwimming; }
    const FVector& GetLastSafeLocation() const { return LastSafeLocation; }

    // --- 冲刺接口 ---

    void EnterSprintMode();
    void ExitSprintMode();
    bool IsSprinting() const { return bIsSprinting; }

    // --- 慢走接口 ---

    void EnterWalkMode();
    void ExitWalkMode();
    bool IsWalking() const { return bIsWalking; }

    // --- 瞄准接口 ---

    void EnterAimMode();
    void ExitAimMode();
    bool IsAiming() const { return bIsAiming; }

    // --- 下落状态接口 ---

    void SetFallingRotationInterpSpeed(float InSpeed);
    float GetFallingRotationInterpSpeed() const { return FallingRotationInterpSpeed; }

protected:
    // --- 物理模拟 ---

    void PhysClimbing(float DeltaTime, int32 Iterations);
    void PhysClimbingCornerTransition(float DeltaTime, int32 Iterations);
    void PhysClimbUp(float DeltaTime, int32 Iterations);
    void PhysGliding(float DeltaTime, int32 Iterations);
    void PhysSwimming(float DeltaTime, int32 Iterations);

    // --- 攀爬状态检测 ---

    void CheckFallingToClimb();
    void CheckGroundedToClimb();
    void CheckClimbToGround();
    void CheckCornerTransition();
    void CheckAndClimbUp();

    // --- 攀爬状态转换 ---

    void DoClimbUp();
    void HandleConvexCorner(const FHitResult& NewWallHit);
    void HandleConcaveCorner(const FHitResult& NewWallHit);

    // --- 攀爬辅助 ---

    bool PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit);
    bool IsWallClimbable(const FVector& WallNormal) const;
    FVector CalculateConvexTargetLocation(const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const;
    FVector CalculateConcaveTargetLocation(const FVector& NewWallHitLocation, const FVector& NewWallNormal) const;
    void ClearClimbState();
    void SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime);

    // --- 游泳辅助 ---

    void ApplySurfaceSnapping(float DeltaTime);
    bool IsStillInWater() const;
    void UpdateLastSafeLocation();

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Climbing|Events")
    FOnClimbUpMontageRequested OnClimbUpMontageRequested;

    // --- 攀爬配置：射线检测 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbTraceDistance = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    FName HeadSocketName = FName("Head");

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    TEnumAsByte<ECollisionChannel> ClimbTraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbUpCheckRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbUpCheckHalfHeight = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    FName UnclimbableActorTag = FName("NotClimbable");

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbSpaceCheckRadius = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbSpaceCheckHalfHeight = 90.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
    float ClimbPredictOffset = 50.0f;

    // --- 攀爬配置：逻辑阈值 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Logic")
    float MinInputDotProduct = -0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Logic")
    float WallNormalZThreshold = 0.2f;

    // --- 攀爬配置：事件 Tag ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Events")
    FGameplayTag TryClimbEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Events")
    FGameplayTag ClimbStopEventTag;

    // --- 攀爬配置：移动 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float ClimbMoveSpeed = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float ClimbRotationInterpSpeed = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float TargetWallDistance = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float ClimbGravityScale = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float WallSnapZOffset = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
    float WallSnapTime = 0.2f;

    // --- 攀爬配置：墙角过渡 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float CornerSweepRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float CornerSweepDistance = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConvexArcRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConcaveSnapDistance = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConvexPositionInterpSpeed = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConvexRotationInterpSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConcavePositionInterpSpeed = 6.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float ConcaveRotationInterpSpeed = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float CornerArrivalThreshold = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
    float CornerRotationArrivalThreshold = 3.0f;

    // --- 攀爬配置：攀爬转地面 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|GroundTransition")
    float GroundDetectDistance = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|GroundTransition")
    float MinClimbHeightAboveGround = 50.0f;

    // --- 攀爬配置：翻越 ---

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
    float ClimbUpPositionInterpSpeed = 200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
    float ClimbUpRotationInterpSpeed = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
    float ClimbUpArrivalThreshold = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
    FVector ClimbUpOffset = FVector(80.0f, 0.0f, 70.0f);

    // --- 攀爬配置：脱墙跳 ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Jump")
    float WallEjectHorizontalSpeed = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Jump")
    float WallEjectVerticalSpeed = 600.0f;

    // --- 滑翔配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    float GlideGravityScale = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    float GlideAirControl = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    FRotator GlideRotationRate = FRotator(0.0f, 100.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    FVector GlideLaunchVelocity = FVector(0.0f, 0.0f, -200.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    float GlideMaxHorizontalSpeed = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    float GlideMinDescentSpeed = -200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
    float GlideMaxDescentSpeed = -50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Gliding|StartCondition", meta = (ClampMin = "0.0"))
    float MinGlideStartHeight = 300.0f;

    // --- 游泳配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
    float SwimMaxSpeed = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
    float FastSwimMaxSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
    float SwimGravityScale = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
    float SwimAirControl = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|SurfaceSnapping")
    float SurfaceSnapOffset = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|SurfaceSnapping")
    float SurfaceSnapInterpSpeed = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Safety")
    float SafeLocationRecordInterval = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Swim|Detection")
    TEnumAsByte<ECollisionChannel> WaterTraceChannel = ECollisionChannel::ECC_GameTraceChannel1;

    // --- 冲刺配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Physics")
    float SprintMaxWalkSpeed = 1200.0f;

    // --- 慢走配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Walk|Physics")
    float WalkMaxWalkSpeed = 200.0f;

    // --- 瞄准配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
    float AimMaxWalkSpeed = 450.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
    bool AimUseControllerRotationYaw = true;

    UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
    bool AimOrientRotationToMovement = false;

    // --- 跳跃非对称重力配置 ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
    float RisingGravityScale = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
    float RisingAirControl = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
    float RisingRotationRate = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Falling")
    float FallingGravityScale = 3.0f;

    // --- 状态事件 Tag ---

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag StopGlideEventTag;

private:
    // --- 攀爬运行时状态 ---

    FVector ClimbWallNormal = FVector::ZeroVector;
    bool bIsSnappingToWall = false;
    bool bCanTryClimb = false;
    FVector SnapTargetLocation = FVector::ZeroVector;
    FRotator SnapTargetRotation = FRotator::ZeroRotator;
    float SnapInterpSpeed = 10.0f;
    bool bIsClimbingUp = false;

    // --- 墙角过渡运行时状态 ---

    FVector CornerTargetLocation = FVector::ZeroVector;
    FVector CornerTargetNormal = FVector::ZeroVector;
    ECornerType CornerType = ECornerType::None;

    // --- 翻越运行时状态 ---

    FVector ClimbUpTargetLocation = FVector::ZeroVector;
    FRotator ClimbUpTargetRotation = FRotator::ZeroRotator;

    // --- 滑翔运行时状态 ---

    float OriginalGravityScale = 1.0f;
    float OriginalAirControl = 0.05f;
    FRotator OriginalRotationRate = FRotator::ZeroRotator;

    // --- 冲刺/慢走运行时状态 ---

    float OriginalMaxWalkSpeed = 600.0f;
    bool bIsSprinting = false;
    bool bIsWalking = false;

    // --- 瞄准运行时状态 ---

    bool bIsAiming = false;
    bool bOriginalOrientRotationToMovement = true;
    bool bOriginalUseControllerRotationYaw = false;

    // --- 游泳运行时状态 ---

    bool bIsFastSwimming = false;
    FVector LastSafeLocation = FVector::ZeroVector;
    float SafeLocationTimer = 0.0f;

    // --- 下落运行时状态 ---

    float FallingRotationInterpSpeed = 3.0f;
};
