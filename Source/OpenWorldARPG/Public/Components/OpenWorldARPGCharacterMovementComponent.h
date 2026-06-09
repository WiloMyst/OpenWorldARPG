// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Types/CustomMovementModeTypes.h"
#include "Types/MovementStateTypes.h"
#include "GameplayTagContainer.h"
#include "OpenWorldARPGCharacterMovementComponent.generated.h"

class UClimbingComponent;
class UAbilitySystemComponent;

/** MovementMode 改变时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnMovementModeChangedDelegate, EMovementMode, PrevMode, EMovementMode, NewMode, uint8, PrevCustomMode, uint8, NewCustomMode);

/**
 * 自定义角色移动组件，为 ECustomMovementMode 提供物理模拟实现。
 * CMC 管"怎么动"（物理/碰撞/网络复制），ActorComponent 管"什么时候动"（检测/状态切换）。
 * PhysCustom 每帧可能执行多次，内部禁止 FindComponentByClass，所有外部指针在初始化阶段缓存。
 */
UCLASS()
class OPENWORLDARPG_API UOpenWorldARPGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UOpenWorldARPGCharacterMovementComponent();

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	// 重写引擎原生的移动模式改变事件
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** MovementMode 改变时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementModeChangedDelegate OnMovementModeChangedDelegate;

	/** 初始化缓存指针 */
	void CacheOwnerReferences();

	// --- 攀爬接口 (供 ClimbingComponent 调用) ---

	void SetClimbWallNormal(const FVector& InNormal);
	void ClearClimbState();
	void SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime);

	// --- 翻越接口 (供 ClimbingComponent 调用) ---

	void SetClimbUpTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation);

	// --- 滑翔接口 (供 GA_GlideBase 调用) ---

	void EnterGlideMode();
	void ExitGlideMode();
	bool IsGliding() const;
	bool IsClimbing() const;
	bool IsFalling() const;
	bool IsGrounded() const;

	// --- 冲刺接口 (供 GA_SprintBase 调用) ---

	void EnterSprintMode();
	void ExitSprintMode();
	bool IsSprinting() const;

	// --- 慢走接口 (供 GA_WalkBase 调用) ---

	void EnterWalkMode();
	void ExitWalkMode();
	bool IsWalking() const;

	// --- 瞄准接口 (供 GA_AimBase 调用) ---

	void EnterAimMode();
	void ExitAimMode();
	bool IsAiming() const;

	// --- 下落状态接口 ---

	void SetFallingRotationInterpSpeed(float InSpeed);
	float GetFallingRotationInterpSpeed() const { return FallingRotationInterpSpeed; }

	// --- 墙角过渡接口 (供 ClimbingComponent 调用) ---

	void SetCornerTransitionTarget(const FVector& InTargetLocation, const FVector& InTargetNormal, ECornerType InCornerType);
	void ClearCornerTransition();
	bool IsInCornerTransition() const;

	// --- 被动运动状态 Tag (CMC 管理，主动行为 Tag 由 GA 的 ActivationOwnedTags 管理) ---

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag AirborneTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag FallingTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag StopGlideEventTag;

	// --- 攀爬配置 ---

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbMoveSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float TargetWallDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbGravityScale = 0.0f;

	// --- 墙角过渡配置 ---

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Convex")
	float ConvexPositionInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Convex")
	float ConvexRotationInterpSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Concave")
	float ConcavePositionInterpSpeed = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Concave")
	float ConcaveRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|General")
	float CornerArrivalThreshold = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|General")
	float CornerRotationArrivalThreshold = 3.0f;

	// --- 地面配置 ---

	UPROPERTY(EditDefaultsOnly, Category = "Grounded|Physics")
	FRotator GroundedRotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// --- 下落配置 ---

	UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
	float FallingAirControl = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
	FRotator FallingRotationRate = FRotator(0.0f, 300.0f, 0.0f);

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

	// --- 翻越配置 ---

	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpPositionInterpSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpArrivalThreshold = 3.0f;

private:
	void PhysClimbing(float DeltaTime, int32 Iterations);
	void PhysCornerTransition(float DeltaTime, int32 Iterations);
	void PhysClimbUp(float DeltaTime, int32 Iterations);
	void PhysGliding(float DeltaTime, int32 Iterations);

	// --- 缓存指针 ---

	UPROPERTY()
	TObjectPtr<ACharacter> CachedOwnerCharacter;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	UPROPERTY()
	TObjectPtr<UClimbingComponent> CachedClimbingComp;

	// --- 攀爬运行时状态 ---

	FVector ClimbWallNormal = FVector::ZeroVector;

	bool bIsSnappingToWall = false;
	FVector SnapTargetLocation = FVector::ZeroVector;
	FRotator SnapTargetRotation = FRotator::ZeroRotator;
	float SnapInterpSpeed = 10.0f;

	float FallingRotationInterpSpeed = 3.0f;

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
};
