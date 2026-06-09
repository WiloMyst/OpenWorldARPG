// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "Types/MovementStateTypes.h"
#include "ClimbingComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UOpenWorldARPGCharacterMovementComponent;
class UAbilitySystemComponent;
class UAnimMontage;
class UCharacterDataAsset;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UClimbingComponent : public UActorComponent
{
    GENERATED_BODY()

    // CMC 在 PhysClimbing 中回写碰撞结果
    friend class UOpenWorldARPGCharacterMovementComponent;

public:
    UClimbingComponent();

    // --- 核心接口 ---

    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void TryClimb();

    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void ExitClimb();

    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void CheckAndClimbUp();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOwnerMovementInput(float InputX, float InputY);

    UFUNCTION()
    void OnInputDetectionTick();

    void EnterClimb(const FHitResult& WallHit);
    void DoClimbUp();

    UFUNCTION()
    void OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void FinishClimbUp();

    bool PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit);

    // --- 下落转攀爬 ---

    void CheckFallingToClimb();

    bool IsWallClimbable(const FVector& WallNormal) const;

    // --- 攀爬转地面 ---

    void CheckClimbToGround();

    // --- 墙角检测与过渡 ---

    void CheckCornerTransition();
    void HandleConvexCorner(const FHitResult& NewWallHit);
    void HandleConcaveCorner(const FHitResult& NewWallHit);
    FVector CalculateConvexTargetLocation(const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const;
    FVector CalculateConcaveTargetLocation(const FVector& NewWallHitLocation, const FVector& NewWallNormal) const;
    void OnCornerTransitionFinished();

protected:
    // --- 配置：降频检测 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Performance")
    float ClimbDetectionInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Performance")
    float FallingToClimbDetectionInterval = 0.1f;

    // --- 配置：射线检测 ---

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

    // --- 配置：逻辑与标签 ---

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

    // --- 配置：攀爬检测 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float ClimbPredictOffset = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckRadius = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckHalfHeight = 90.0f;

    // --- 配置：表现与动画 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapZOffset = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapTime = 0.2f;

    // --- 配置：墙角过渡 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float CornerSweepRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float CornerSweepDistance = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float ConvexArcRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float ConcaveSnapDistance = 30.0f;

    // --- 配置：攀爬转地面 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|GroundTransition")
    float GroundDetectDistance = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|GroundTransition")
    float MinClimbHeightAboveGround = 50.0f;

private:
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> MovementComp;

    UPROPERTY()
    TObjectPtr<UOpenWorldARPGCharacterMovementComponent> CustomMovementComp;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC;

    UPROPERTY()
    TObjectPtr<UCharacterDataAsset> CharacterData;

    bool bIsClimbingUp = false;
    FVector CurrentWallNormal = FVector::ZeroVector;

    FTimerHandle ClimbDetectionTimerHandle;
    FTimerHandle FallingToClimbTimerHandle;
    FTimerHandle InputDetectionTimerHandle;

    FVector CMCHitWallNormal = FVector::ZeroVector;
    bool bCMCHitWall = false;

    UFUNCTION()
    void OnClimbDetectionTick();

    UFUNCTION()
    void OnFallingToClimbTick();
};
