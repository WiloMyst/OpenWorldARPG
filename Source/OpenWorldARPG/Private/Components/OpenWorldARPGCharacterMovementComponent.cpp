// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/ClimbingComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

UOpenWorldARPGCharacterMovementComponent::UOpenWorldARPGCharacterMovementComponent()
{
}

void UOpenWorldARPGCharacterMovementComponent::CacheOwnerReferences()
{
	CachedOwnerCharacter = Cast<ACharacter>(GetOwner());
	if (CachedOwnerCharacter)
	{
		CachedClimbingComp = CachedOwnerCharacter->FindComponentByClass<UClimbingComponent>();
	}
}

void UOpenWorldARPGCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// 根据 CustomMovementMode 分发到对应的物理函数
	switch (static_cast<ECustomMovementMode>(CustomMovementMode))
	{
	case ECustomMovementMode::Climbing:
		PhysClimbing(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::ClimbingCornerTransition:
		PhysCornerTransition(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::ClimbUp:
		PhysClimbUp(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Gliding:
		// TODO: 滑翔物理
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Swimming:
		// TODO: 游泳物理
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	default:
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	}
}

// ==========================================
// 攀爬接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::SetClimbWallNormal(const FVector& InNormal)
{
	ClimbWallNormal = InNormal;
}

void UOpenWorldARPGCharacterMovementComponent::ClearClimbState()
{
	ClimbWallNormal = FVector::ZeroVector;
	bIsSnappingToWall = false;
}

void UOpenWorldARPGCharacterMovementComponent::SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime)
{
	bIsSnappingToWall = true;
	SnapTargetLocation = InTargetLocation;
	SnapTargetRotation = InTargetRotation;
	// 从期望吸附时间反算插值速率：VInterpTo 在 time=T 时到达约 63%
	// 要在 SnapTime 内完成吸附，需要 InterpSpeed ≈ 1/SnapTime * 缩放因子
	SnapInterpSpeed = (InSnapTime > 0.0f) ? (3.0f / InSnapTime) : 30.0f;
}

// ==========================================
// 翻越接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::SetClimbUpTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation)
{
	ClimbUpTargetLocation = InTargetLocation;
	ClimbUpTargetRotation = InTargetRotation;
}

// ==========================================
// 下落状态接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::SetFallingRotationInterpSpeed(float InSpeed)
{
	FallingRotationInterpSpeed = InSpeed;
}

// ==========================================
// 墙角过渡接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::SetCornerTransitionTarget(const FVector& InTargetLocation, const FVector& InTargetNormal, ECornerType InCornerType)
{
	CornerTargetLocation = InTargetLocation;
	CornerTargetNormal = InTargetNormal;
	CornerType = InCornerType;
	// 状态由 CustomMovementMode 标识，切换到 ClimbingCornerTransition
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbingCornerTransition));
}

void UOpenWorldARPGCharacterMovementComponent::ClearCornerTransition()
{
	CornerTargetLocation = FVector::ZeroVector;
	CornerTargetNormal = FVector::ZeroVector;
	CornerType = ECornerType::None;
	// 恢复到 Climbing 模式
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
}

bool UOpenWorldARPGCharacterMovementComponent::IsInCornerTransition() const
{
	return MovementMode == MOVE_Custom
		&& static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::ClimbingCornerTransition;
}

// ==========================================
// 攀爬物理模拟
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	// 使用缓存指针，不再每帧 Cast/FindComponentByClass
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		// 降级：仅在缓存失效时才做 Cast（极端情况）
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		// ==========================================
		// 攀爬吸附阶段 (替代 MoveComponentTo)
		// 在 PhysClimbing 的物理安全框架下进行插值吸附
		// ==========================================
		if (bIsSnappingToWall)
		{
			Velocity = FVector::ZeroVector;

			FVector CurrentLoc = Char->GetActorLocation();
			FVector NewLoc = FMath::VInterpTo(CurrentLoc, SnapTargetLocation, DeltaTime, SnapInterpSpeed);
			FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), SnapTargetRotation, DeltaTime, SnapInterpSpeed);

			FHitResult Hit;
			SafeMoveUpdatedComponent(NewLoc - CurrentLoc, NewRot.Quaternion(), true, Hit, ETeleportType::None);

			// 吸附完成检测
			if (FVector::DistSquared(Char->GetActorLocation(), SnapTargetLocation) < 4.0f
				&& Char->GetActorRotation().Equals(SnapTargetRotation, 1.0f))
			{
				bIsSnappingToWall = false;
			}

			// 吸附期间不处理移动输入
			ApplyRootMotionToVelocity(DeltaTime);
			Iterations++;
			bJustTeleported = false;
			return;
		}

		if (ClimbGravityScale > 0.0f)
		{
			Velocity.Z += GetGravityZ() * ClimbGravityScale * DeltaTime;
		}
		else
		{
			Velocity = FVector::ZeroVector;
		}

		// ==========================================
		// 使用 Acceleration 获取输入（由 TickComponent → ControlledCharacterMove 设置）
		// 注意：不能用 ConsumeInputVector()，因为 TickComponent 已经消费了 PendingInputVector
		// ==========================================
		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero() && !ClimbWallNormal.IsNearlyZero())
		{
			// 将世界空间输入向量投影到墙面切平面
			FVector ProjectedInput = FVector::VectorPlaneProject(InputVector, ClimbWallNormal).GetSafeNormal();
			if (!ProjectedInput.IsNearlyZero())
			{
				Velocity = ProjectedInput * ClimbMoveSpeed;
			}
		}
	}

	ApplyRootMotionToVelocity(DeltaTime);
	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);

		if (!Hit.bStartPenetrating)
		{
			ClimbWallNormal = Hit.Normal;

			// 回写碰撞结果给 ClimbingComponent (使用缓存指针)
			if (UClimbingComponent* ClimbComp = CachedClimbingComp.Get())
			{
				ClimbComp->CMCHitWallNormal = Hit.Normal;
				ClimbComp->bCMCHitWall = true;
			}
		}
	}

	// 朝向：始终面向墙壁
	if (!ClimbWallNormal.IsNearlyZero())
	{
		FRotator TargetRot = FRotationMatrix::MakeFromX(-ClimbWallNormal).Rotator();
		FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), TargetRot, DeltaTime, ClimbRotationInterpSpeed);
		Char->SetActorRotation(NewRot);
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

// ==========================================
// 墙角过渡物理模拟
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::PhysCornerTransition(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 墙角过渡期间：零速度，完全由插值驱动
	Velocity = FVector::ZeroVector;

	// 位置插值
	float PosInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexPositionInterpSpeed : ConcavePositionInterpSpeed;
	FVector CurrentLocation = Char->GetActorLocation();
	FVector NewLocation = FMath::VInterpTo(CurrentLocation, CornerTargetLocation, DeltaTime, PosInterpSpeed);

	// 旋转插值：面向目标法线的反方向
	float RotInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexRotationInterpSpeed : ConcaveRotationInterpSpeed;
	FRotator TargetRot = FRotationMatrix::MakeFromX(-CornerTargetNormal).Rotator();
	FRotator CurrentRot = Char->GetActorRotation();
	FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, RotInterpSpeed);

	// 应用位移
	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 检测过渡是否完成
	float DistSq = FVector::DistSquared(Char->GetActorLocation(), CornerTargetLocation);
	float RotDiff = NewRot.Equals(TargetRot, CornerRotationArrivalThreshold) ? 0.0f : 1.0f;

	if (DistSq < FMath::Square(CornerArrivalThreshold) && RotDiff < 1.0f)
	{
		// 过渡完成：恢复 Climbing 模式并立即通知 ClimbingComponent
		ClearCornerTransition(); // 这会将 CustomMovementMode 恢复为 Climbing
		if (UClimbingComponent* ClimbComp = CachedClimbingComp.Get())
		{
			ClimbComp->OnCornerTransitionFinished();
		}
	}
}

// ==========================================
// 翻越物理模拟
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::PhysClimbUp(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 翻越期间：零速度，完全由插值驱动
	Velocity = FVector::ZeroVector;

	// 位置插值
	FVector CurrentLocation = Char->GetActorLocation();
	FVector NewLocation = FMath::VInterpTo(CurrentLocation, ClimbUpTargetLocation, DeltaTime, ClimbUpPositionInterpSpeed);

	// 旋转插值
	FRotator CurrentRot = Char->GetActorRotation();
	FRotator NewRot = FMath::RInterpTo(CurrentRot, ClimbUpTargetRotation, DeltaTime, ClimbUpRotationInterpSpeed);

	// 应用位移
	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 翻越到达目标后，等待 ClimbingComponent 的蒙太奇回调来恢复状态
	// CMC 不主动切换模式，只负责物理位移
}
