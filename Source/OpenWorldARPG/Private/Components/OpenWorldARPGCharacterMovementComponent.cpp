// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"

UOpenWorldARPGCharacterMovementComponent::UOpenWorldARPGCharacterMovementComponent()
{
}

void UOpenWorldARPGCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	DefaultGravityScale = GravityScale;
	DefaultAirControl = AirControl;
}

void UOpenWorldARPGCharacterMovementComponent::CacheOwnerReferences()
{
	CachedOwnerCharacter = Cast<ACharacter>(GetOwner());
	if (CachedOwnerCharacter)
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedOwnerCharacter);
	}
}

// ============================================================================
// 引擎重写
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	// Falling：非对称重力 + 攀爬检测
	if (MovementMode == MOVE_Falling)
	{
		if (Velocity.Z > 0.0f)
		{
			GravityScale = RisingGravityScale;
			AirControl = RisingAirControl;
			RotationRate = FRotator(0.0f, RisingRotationRate, 0.0f);
			bOrientRotationToMovement = true;
		}
		else
		{
			GravityScale = FallingGravityScale;
			AirControl = FallingAirControl;
			bOrientRotationToMovement = false;
		}
		CheckFallingToClimb();
	}

	// Walking：走墙攀爬检测
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		CheckGroundedToClimb();
	}

	// 移动状态 Tag（根据输入意愿）
	if (CachedASC && MovingTag.IsValid())
	{
		bool bHasMovementInput = Acceleration.Size2D() > MovingSpeedThreshold;
		if (bHasMovementInput)
			CachedASC->AddLooseGameplayTag(MovingTag);
		else
			CachedASC->RemoveLooseGameplayTag(MovingTag);
	}

	// Climbing：持续检测（落地 > 墙角 > 法线更新 > 翻越）
	if (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Climbing)
	{
		CheckClimbToGround();
		if (MovementMode != MOVE_Custom) return;

		CheckCornerTransition();
		if (MovementMode != MOVE_Custom
			|| static_cast<ECustomMovementMode>(CustomMovementMode) != ECustomMovementMode::Climbing) return;

		FHitResult ChestHit, HeadHit;
		if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
		{
			ClimbWallNormal = ChestHit.Normal;
		}

		CheckAndClimbUp();
	}
}

void UOpenWorldARPGCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (static_cast<ECustomMovementMode>(CustomMovementMode))
	{
	case ECustomMovementMode::Climbing:
		PhysClimbing(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::ClimbingCornerTransition:
		PhysClimbingCornerTransition(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::ClimbUp:
		PhysClimbUp(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Gliding:
		PhysGliding(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Swimming:
		PhysSwimming(DeltaTime, Iterations);
		break;
	default:
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	}
}

void UOpenWorldARPGCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// 离开自定义模式时恢复物理参数
	bool bWasGliding = (PreviousMovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(PreviousCustomMode) == ECustomMovementMode::Gliding);
	bool bIsStillGliding = (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding);

	bool bWasSwimming = (PreviousMovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(PreviousCustomMode) == ECustomMovementMode::Swimming);
	bool bIsStillSwimming = (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Swimming);

	if (bWasGliding && !bIsStillGliding)
	{
		GravityScale = OriginalGravityScale;
		AirControl = OriginalAirControl;
		RotationRate = OriginalRotationRate;
	}

	if (bWasSwimming && !bIsStillSwimming)
	{
		GravityScale = OriginalGravityScale;
		AirControl = OriginalAirControl;
		RotationRate = OriginalRotationRate;
		bOrientRotationToMovement = true;
		bIsFastSwimming = false;
	}

	// 非对称重力恢复
	if (PreviousMovementMode == MOVE_Falling && MovementMode != MOVE_Falling)
	{
		GravityScale = DefaultGravityScale;
		AirControl = DefaultAirControl;
		bOrientRotationToMovement = true;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// GAS Tag 映射
	if (CachedASC)
	{
		if (AirborneTag.IsValid())      CachedASC->RemoveLooseGameplayTag(AirborneTag);
		if (FallingTag.IsValid())       CachedASC->RemoveLooseGameplayTag(FallingTag);
		if (SwimmingTag.IsValid())      CachedASC->RemoveLooseGameplayTag(SwimmingTag);
		if (FastSwimmingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(FastSwimmingTag);
		if (MovingTag.IsValid())        CachedASC->RemoveLooseGameplayTag(MovingTag);

		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
		{
			AirControl = 0.0f;
			RotationRate = GroundedRotationRate;
			bOrientRotationToMovement = true;

			UpdateLastSafeLocation();

			if (PreviousMovementMode == MOVE_Falling || PreviousMovementMode == MOVE_Custom)
			{
				if (StopGlideEventTag.IsValid() && CachedOwnerCharacter)
				{
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CachedOwnerCharacter.Get(), StopGlideEventTag, FGameplayEventData());
				}
			}
		}
		else if (MovementMode == MOVE_Falling)
		{
			if (AirborneTag.IsValid()) CachedASC->AddLooseGameplayTag(AirborneTag);
			if (FallingTag.IsValid())  CachedASC->AddLooseGameplayTag(FallingTag);

			AirControl = FallingAirControl;
			RotationRate = FallingRotationRate;
			bOrientRotationToMovement = true;
		}
		else if (MovementMode == MOVE_Custom)
		{
			const ECustomMovementMode CurrCustom = static_cast<ECustomMovementMode>(CustomMovementMode);

			if (CurrCustom == ECustomMovementMode::Gliding)
			{
				if (AirborneTag.IsValid()) CachedASC->AddLooseGameplayTag(AirborneTag);
			}
			else if (CurrCustom == ECustomMovementMode::Swimming)
			{
				if (SwimmingTag.IsValid()) CachedASC->AddLooseGameplayTag(SwimmingTag);
			}
		}
	}

	OnMovementModeChangedDelegate.Broadcast(PreviousMovementMode, MovementMode.GetValue(), PreviousCustomMode, CustomMovementMode);
}

// ============================================================================
// 攀爬 - 公开接口
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::TryClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	if (!IsGrounded() && !IsFalling()) return;

	FHitResult ChestHit;
	if (DetectClimbableWall(ChestHit))
	{
		if (TryClimbEventTag.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = TryClimbEventTag;
			Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(ChestHit);
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Char, TryClimbEventTag, Payload);
		}
	}
}

void UOpenWorldARPGCharacterMovementComponent::EnterClimb(const FHitResult& WallHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	ClimbWallNormal = WallHit.Normal;

	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
	bOrientRotationToMovement = false;

	const FRotator TargetRot = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);
	const FRotator FinalRot = FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.0f);
	const FVector TargetLoc = Char->GetActorLocation() + FVector(0.0f, 0.0f, WallSnapZOffset);
	SetClimbSnapTarget(TargetLoc, FinalRot, WallSnapTime);
}

void UOpenWorldARPGCharacterMovementComponent::ExitClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	ClearClimbState();
	SetMovementMode(MOVE_Falling);
	bOrientRotationToMovement = true;

	FRotator CurrentRot = Char->GetActorRotation();
	Char->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
}

void UOpenWorldARPGCharacterMovementComponent::TryClimbUp()
{
	if (!CachedOwnerCharacter.Get() || !IsClimbing()) return;
	CheckAndClimbUp();
}

void UOpenWorldARPGCharacterMovementComponent::FinishClimbUp()
{
	if (!bIsClimbingUp) return;

	bIsClimbingUp = false;
	ClearClimbState();
	SetMovementMode(MOVE_Falling);
	bOrientRotationToMovement = true;

	if (ACharacter* Char = CachedOwnerCharacter.Get())
	{
		FRotator CurrentRot = Char->GetActorRotation();
		Char->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
	}
}

void UOpenWorldARPGCharacterMovementComponent::DoWallEject()
{
	if (!IsClimbing()) return;

	const FVector EjectVelocity = (ClimbWallNormal * WallEjectHorizontalSpeed) + (FVector::UpVector * WallEjectVerticalSpeed);
	ExitClimb();
	Velocity = EjectVelocity;
	UpdateComponentVelocity();
}

bool UOpenWorldARPGCharacterMovementComponent::DetectClimbableWall(FHitResult& OutChestHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return false;
	if (Acceleration.IsNearlyZero()) return false;

	FHitResult HeadHit;
	if (PerformClimbTraces(FVector::ZeroVector, OutChestHit, HeadHit))
	{
		if (OutChestHit.GetActor() && OutChestHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return false;
		if (!IsWallClimbable(OutChestHit.Normal)) return false;

		float DotResult = FVector::DotProduct(Acceleration, OutChestHit.Normal);
		if (DotResult < MinInputDotProduct)
		{
			return true;
		}
	}

	return false;
}

FTransform UOpenWorldARPGCharacterMovementComponent::CalculateClimbWarpTarget(const FHitResult& WallHit) const
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char || !WallHit.bBlockingHit) return FTransform::Identity;

	const FVector TargetLocation = WallHit.Location + WallHit.Normal * TargetWallDistance;
	const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);

	return FTransform(TargetRotation, TargetLocation, FVector::OneVector);
}

bool UOpenWorldARPGCharacterMovementComponent::IsClimbing() const
{
	if (MovementMode != MOVE_Custom) return false;
	const ECustomMovementMode CurrCustom = static_cast<ECustomMovementMode>(CustomMovementMode);
	return CurrCustom == ECustomMovementMode::Climbing || CurrCustom == ECustomMovementMode::ClimbingCornerTransition;
}

bool UOpenWorldARPGCharacterMovementComponent::IsInCornerTransition() const
{
	return MovementMode == MOVE_Custom
		&& static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::ClimbingCornerTransition;
}

// ============================================================================
// 攀爬 - 状态检测
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::CheckFallingToClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char || !Char->IsLocallyControlled()) return;

	if (Acceleration.IsNearlyZero())
	{
		bCanTryClimb = false;
		return;
	}

	FHitResult ChestHit, HeadHit;
	if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
	{
		if (ChestHit.GetActor() && ChestHit.GetActor()->ActorHasTag(UnclimbableActorTag))
		{
			bCanTryClimb = false;
			return;
		}
		if (!IsWallClimbable(ChestHit.Normal))
		{
			bCanTryClimb = false;
			return;
		}

		float DotResult = FVector::DotProduct(Acceleration, ChestHit.Normal);
		if (DotResult < MinInputDotProduct)
		{
			if (!bCanTryClimb)
			{
				bCanTryClimb = true;

				if (TryClimbEventTag.IsValid())
				{
					FGameplayEventData Payload;
					Payload.EventTag = TryClimbEventTag;
					Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(ChestHit);
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Char, TryClimbEventTag, Payload);
				}
			}
			return;
		}
	}

	bCanTryClimb = false;
}

void UOpenWorldARPGCharacterMovementComponent::CheckGroundedToClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char || !Char->IsLocallyControlled()) return;

	FHitResult ChestHit;
	if (DetectClimbableWall(ChestHit))
	{
		if (!bCanTryClimb)
		{
			bCanTryClimb = true;

			if (TryClimbEventTag.IsValid())
			{
				FGameplayEventData Payload;
				Payload.EventTag = TryClimbEventTag;
				Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(ChestHit);
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Char, TryClimbEventTag, Payload);
			}
		}
		return;
	}

	bCanTryClimb = false;
}

void UOpenWorldARPGCharacterMovementComponent::CheckClimbToGround()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	const FVector Start = Char->GetActorLocation();
	const float CapsuleHalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector End = Start - FVector(0.0f, 0.0f, GroundDetectDistance + CapsuleHalfHeight);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ClimbTraceChannel, Params)
		&& HitResult.bBlockingHit)
	{
		const float FeetHeightAboveGround = HitResult.Distance - CapsuleHalfHeight;
		if (FeetHeightAboveGround < MinClimbHeightAboveGround) return;

		if (ClimbStopEventTag.IsValid())
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Char, ClimbStopEventTag, FGameplayEventData());
		}
	}
}

void UOpenWorldARPGCharacterMovementComponent::CheckCornerTransition()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char || ClimbWallNormal.IsNearlyZero()) return;
	if (Acceleration.IsNearlyZero()) return;

	const FVector ActorRight = Char->GetActorRightVector();
	const float LateralInput = FVector::DotProduct(Acceleration, ActorRight);
	if (FMath::IsNearlyZero(LateralInput)) return;
	if (IsInCornerTransition()) return;

	const FVector ActorLocation = Char->GetActorLocation();
	const FVector SideDir = ActorRight * FMath::Sign(LateralInput);
	const FVector SweepStart = ActorLocation;
	const FVector SweepEnd = ActorLocation + SideDir * CornerSweepDistance;

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(CornerSweepRadius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	FHitResult SweepHit;
	if (GetWorld()->SweepSingleByChannel(SweepHit, SweepStart, SweepEnd, FQuat::Identity, ClimbTraceChannel, SphereShape, Params)
		&& SweepHit.bBlockingHit)
	{
		if (SweepHit.GetActor() && SweepHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return;
		if (!IsWallClimbable(SweepHit.Normal)) return;

		const float NormalDot = FVector::DotProduct(ClimbWallNormal, SweepHit.Normal);

		if (NormalDot < 0.0f)
		{
			HandleConvexCorner(SweepHit);
		}
		else if (NormalDot < 0.99f)
		{
			HandleConcaveCorner(SweepHit);
		}
	}
}

void UOpenWorldARPGCharacterMovementComponent::CheckAndClimbUp()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char || bIsClimbingUp) return;

	const FVector StartLoc = Char->GetActorLocation()
		+ (Char->GetActorForwardVector() * ClimbPredictOffset)
		+ FVector(0.0f, 0.0f, ClimbUpCheckHalfHeight);

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(ClimbUpCheckRadius, ClimbUpCheckHalfHeight);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	FHitResult HitResult;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult, StartLoc, StartLoc, FQuat::Identity, ClimbTraceChannel, CapsuleShape, Params);

	if (!bHit)
	{
		DoClimbUp();
	}
}

// ============================================================================
// 攀爬 - 状态转换
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::DoClimbUp()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	bIsClimbingUp = true;

	const FVector TargetLoc = Char->GetActorLocation()
		+ Char->GetActorForwardVector() * ClimbUpOffset.X
		+ Char->GetActorRightVector() * ClimbUpOffset.Y
		+ FVector(0.0f, 0.0f, ClimbUpOffset.Z);
	const FRotator TargetRot = FRotator(0.0f, Char->GetActorRotation().Yaw, 0.0f);

	ClimbUpTargetLocation = TargetLoc;
	ClimbUpTargetRotation = TargetRot;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbUp));

	OnClimbUpMontageRequested.Broadcast(nullptr);
}

void UOpenWorldARPGCharacterMovementComponent::HandleConvexCorner(const FHitResult& NewWallHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	const FVector CornerPoint = NewWallHit.Location;
	const FVector TargetLocation = CalculateConvexTargetLocation(CornerPoint, ClimbWallNormal, NewWallHit.Normal);

	CornerTargetLocation = TargetLocation;
	CornerTargetNormal = NewWallHit.Normal;
	CornerType = ECornerType::Convex;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbingCornerTransition));
}

void UOpenWorldARPGCharacterMovementComponent::HandleConcaveCorner(const FHitResult& NewWallHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	const FVector TargetLocation = CalculateConcaveTargetLocation(NewWallHit.Location, NewWallHit.Normal);

	CornerTargetLocation = TargetLocation;
	CornerTargetNormal = NewWallHit.Normal;
	CornerType = ECornerType::Concave;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbingCornerTransition));
}

// ============================================================================
// 攀爬 - 物理模拟
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 墙面吸附阶段
	if (bIsSnappingToWall)
	{
		Velocity = FVector::ZeroVector;

		const FVector CurrentLocation = Char->GetActorLocation();
		const FVector NewLocation = FMath::VInterpTo(CurrentLocation, SnapTargetLocation, DeltaTime, SnapInterpSpeed);

		const FRotator CurrentRot = Char->GetActorRotation();
		const FRotator NewRot = FMath::RInterpTo(CurrentRot, SnapTargetRotation, DeltaTime, SnapInterpSpeed);

		FHitResult Hit;
		SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
			SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}

		if (FVector::DistSquared(Char->GetActorLocation(), SnapTargetLocation) < 4.0f)
		{
			bIsSnappingToWall = false;
		}
		return;
	}

	// 正常攀爬移动
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		if (ClimbGravityScale > 0.0f)
		{
			Velocity.Z += GetGravityZ() * ClimbGravityScale * DeltaTime;
		}
		else
		{
			Velocity = FVector::ZeroVector;
		}

		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero() && !ClimbWallNormal.IsNearlyZero())
		{
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

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);

		if (!Hit.bStartPenetrating)
		{
			ClimbWallNormal = Hit.Normal;
		}
	}

	// 朝向墙壁
	if (!ClimbWallNormal.IsNearlyZero())
	{
		const FRotator TargetRot = FRotationMatrix::MakeFromX(-ClimbWallNormal).Rotator();
		const FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), TargetRot, DeltaTime, ClimbRotationInterpSpeed);
		Char->SetActorRotation(NewRot);
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

void UOpenWorldARPGCharacterMovementComponent::PhysClimbingCornerTransition(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	Velocity = FVector::ZeroVector;

	const float PosInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexPositionInterpSpeed : ConcavePositionInterpSpeed;
	const FVector CurrentLocation = Char->GetActorLocation();
	const FVector NewLocation = FMath::VInterpTo(CurrentLocation, CornerTargetLocation, DeltaTime, PosInterpSpeed);

	const float RotInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexRotationInterpSpeed : ConcaveRotationInterpSpeed;
	const FRotator TargetRot = FRotationMatrix::MakeFromX(-CornerTargetNormal).Rotator();
	const FRotator CurrentRot = Char->GetActorRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, RotInterpSpeed);

	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (FVector::DistSquared(Char->GetActorLocation(), CornerTargetLocation) < FMath::Square(CornerArrivalThreshold)
		&& NewRot.Equals(TargetRot, CornerRotationArrivalThreshold))
	{
		CornerTargetLocation = FVector::ZeroVector;
		CornerTargetNormal = FVector::ZeroVector;
		CornerType = ECornerType::None;
		ClimbWallNormal = CornerTargetNormal;
		SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
	}
}

void UOpenWorldARPGCharacterMovementComponent::PhysClimbUp(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	Velocity = FVector::ZeroVector;

	const FVector CurrentLocation = Char->GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, ClimbUpTargetLocation, DeltaTime, ClimbUpPositionInterpSpeed);

	const FRotator CurrentRot = Char->GetActorRotation();
	const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, ClimbUpTargetRotation, DeltaTime, ClimbUpRotationInterpSpeed);

	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 等待 Character 蒙太奇回调调用 FinishClimbUp
}

// ============================================================================
// 攀爬 - 辅助
// ============================================================================

bool UOpenWorldARPGCharacterMovementComponent::PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return false;

	const FVector Forward = Char->GetActorForwardVector();
	const FVector ChestStart = Char->GetActorLocation() + TraceOffset;
	const FVector ChestEnd = ChestStart + (Forward * ClimbTraceDistance);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	const bool bChestHit = GetWorld()->LineTraceSingleByChannel(OutChestHit, ChestStart, ChestEnd, ClimbTraceChannel, Params);

	if (USkeletalMeshComponent* Mesh = Char->GetMesh())
	{
		const FVector HeadStart = Mesh->GetSocketLocation(HeadSocketName) + TraceOffset;
		const FVector HeadEnd = HeadStart + (Forward * ClimbTraceDistance);
		GetWorld()->LineTraceSingleByChannel(OutHeadHit, HeadStart, HeadEnd, ClimbTraceChannel, Params);
	}

	return bChestHit;
}

bool UOpenWorldARPGCharacterMovementComponent::IsWallClimbable(const FVector& WallNormal) const
{
	return FMath::Abs(WallNormal.Z) < WallNormalZThreshold;
}

FVector UOpenWorldARPGCharacterMovementComponent::CalculateConvexTargetLocation(
	const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const
{
	const FVector Bisector = (CurrentNormal + NewNormal).GetSafeNormal();
	FVector TargetLoc = CornerPoint + Bisector * ConvexArcRadius;

	if (ACharacter* Char = CachedOwnerCharacter.Get())
	{
		TargetLoc.Z = Char->GetActorLocation().Z;
	}

	return TargetLoc;
}

FVector UOpenWorldARPGCharacterMovementComponent::CalculateConcaveTargetLocation(
	const FVector& NewWallHitLocation, const FVector& NewWallNormal) const
{
	FVector TargetLoc = NewWallHitLocation + NewWallNormal * ConcaveSnapDistance;

	if (ACharacter* Char = CachedOwnerCharacter.Get())
	{
		TargetLoc.Z = Char->GetActorLocation().Z;
	}

	return TargetLoc;
}

void UOpenWorldARPGCharacterMovementComponent::ClearClimbState()
{
	ClimbWallNormal = FVector::ZeroVector;
	bIsSnappingToWall = false;
	bIsClimbingUp = false;
	bCanTryClimb = false;
}

void UOpenWorldARPGCharacterMovementComponent::SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime)
{
	bIsSnappingToWall = true;
	SnapTargetLocation = InTargetLocation;
	SnapTargetRotation = InTargetRotation;
	SnapInterpSpeed = (InSnapTime > 0.0f) ? (3.0f / InSnapTime) : 30.0f;
}

// ============================================================================
// 滑翔
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::EnterGlideMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	OriginalGravityScale = GravityScale;
	OriginalAirControl = AirControl;
	OriginalRotationRate = RotationRate;

	GravityScale = GlideGravityScale;
	AirControl = GlideAirControl;
	RotationRate = GlideRotationRate;
	bOrientRotationToMovement = true;

	Velocity = FVector(GlideLaunchVelocity.X, GlideLaunchVelocity.Y, GlideLaunchVelocity.Z);

	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Gliding));
}

void UOpenWorldARPGCharacterMovementComponent::ExitGlideMode()
{
	SetMovementMode(MOVE_Falling);
}

float UOpenWorldARPGCharacterMovementComponent::GetDistanceToGround() const
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return -1.0f;
	}

	const FVector Start = Char->GetActorLocation();
	const FVector End = Start - FVector::UpVector * 2000.0f;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Char);

	if (Char->GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
	{
		return HitResult.Distance;
	}

	return -1.0f;
}

void UOpenWorldARPGCharacterMovementComponent::PhysGliding(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		// 微量重力模拟滑翔下沉
		Velocity.Z += GetGravityZ() * 0.1f * DeltaTime;

		// 水平速度限制
		FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
		if (HorizontalVel.SizeSquared() > FMath::Square(GlideMaxHorizontalSpeed))
		{
			HorizontalVel = HorizontalVel.GetSafeNormal() * GlideMaxHorizontalSpeed;
		}

		Velocity.Z = FMath::Clamp(Velocity.Z, GlideMinDescentSpeed, GlideMaxDescentSpeed);
		Velocity = FVector(HorizontalVel.X, HorizontalVel.Y, Velocity.Z);

		// 输入加速
		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero())
		{
			const FVector InputDir = InputVector.GetSafeNormal();
			const float GlideAcceleration = 800.0f;
			Velocity += FVector(InputDir.X, InputDir.Y, 0.0f) * GlideAcceleration * DeltaTime;
		}

		// 空气阻力
		HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
		if (HorizontalVel.SizeSquared() > FMath::Square(GlideMaxHorizontalSpeed))
		{
			HorizontalVel = HorizontalVel.GetSafeNormal() * GlideMaxHorizontalSpeed;
			Velocity = FVector(HorizontalVel.X, HorizontalVel.Y, Velocity.Z);
		}
	}

	ApplyRootMotionToVelocity(DeltaTime);
	Iterations++;
	bJustTeleported = false;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	// 朝向移动方向
	FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
	if (!HorizontalVel.IsNearlyZero())
	{
		const FRotator TargetRot = HorizontalVel.GetSafeNormal().Rotation();
		const FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), TargetRot, DeltaTime, 5.0f);
		Char->SetActorRotation(NewRot);
	}

	// 着地检测
	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
	if (FloorResult.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Walking);
	}
}

// ============================================================================
// 游泳
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::EnterSwimMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	OriginalGravityScale = GravityScale;
	OriginalAirControl = AirControl;
	OriginalRotationRate = RotationRate;

	GravityScale = SwimGravityScale;
	AirControl = SwimAirControl;
	MaxFlySpeed = SwimMaxSpeed;
	bOrientRotationToMovement = true;

	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Swimming));
}

void UOpenWorldARPGCharacterMovementComponent::ExitSwimMode()
{
	SetMovementMode(MOVE_Falling);
}

void UOpenWorldARPGCharacterMovementComponent::EnterFastSwimMode()
{
	if (bIsFastSwimming) return;
	bIsFastSwimming = true;
	MaxFlySpeed = FastSwimMaxSpeed;
	if (CachedASC && FastSwimmingTag.IsValid())
	{
		CachedASC->AddLooseGameplayTag(FastSwimmingTag);
	}
}

void UOpenWorldARPGCharacterMovementComponent::ExitFastSwimMode()
{
	if (!bIsFastSwimming) return;
	bIsFastSwimming = false;
	MaxFlySpeed = SwimMaxSpeed;
	if (CachedASC && FastSwimmingTag.IsValid())
	{
		CachedASC->RemoveLooseGameplayTag(FastSwimmingTag);
	}
}

void UOpenWorldARPGCharacterMovementComponent::PhysSwimming(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (!IsStillInWater())
	{
		ExitSwimMode();
		return;
	}

	ApplySurfaceSnapping(DeltaTime);

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity.Z = 0.0f;

		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero())
		{
			FVector HorizontalInput = FVector(InputVector.X, InputVector.Y, 0.0f).GetSafeNormal();
			const float CurrentMaxSpeed = bIsFastSwimming ? FastSwimMaxSpeed : SwimMaxSpeed;
			Velocity = HorizontalInput * CurrentMaxSpeed;
		}
		else
		{
			FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
			HorizontalVel = FMath::VInterpTo(HorizontalVel, FVector::ZeroVector, DeltaTime, 8.0f);
			Velocity = HorizontalVel;
		}
	}

	ApplyRootMotionToVelocity(DeltaTime);
	Iterations++;
	bJustTeleported = false;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	// 朝向
	FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
	if (!HorizontalVel.IsNearlyZero())
	{
		const FRotator TargetRot = HorizontalVel.GetSafeNormal().Rotation();
		const FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), TargetRot, DeltaTime, 8.0f);
		Char->SetActorRotation(NewRot);
	}

	SafeLocationTimer += DeltaTime;
}

void UOpenWorldARPGCharacterMovementComponent::ApplySurfaceSnapping(float DeltaTime)
{
	// 预留：Water Plugin 集成后实现水面吸附
}

bool UOpenWorldARPGCharacterMovementComponent::IsStillInWater() const
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return false;

	const FVector Location = Char->GetActorLocation();
	const float CapsuleRadius = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	return GetWorld()->OverlapAnyTestByChannel(Location, FQuat::Identity, WaterTraceChannel, Shape, Params);
}

void UOpenWorldARPGCharacterMovementComponent::UpdateLastSafeLocation()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		if (SafeLocationTimer >= SafeLocationRecordInterval || LastSafeLocation.IsZero())
		{
			LastSafeLocation = Char->GetActorLocation();
			SafeLocationTimer = 0.0f;
		}
	}
}

// ============================================================================
// 冲刺
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::EnterSprintMode()
{
	if (bIsWalking) ExitWalkMode();
	if (!bIsSprinting) OriginalMaxWalkSpeed = MaxWalkSpeed;
	bIsSprinting = true;
	MaxWalkSpeed = SprintMaxWalkSpeed;
}

void UOpenWorldARPGCharacterMovementComponent::ExitSprintMode()
{
	if (!bIsSprinting) return;
	bIsSprinting = false;
	MaxWalkSpeed = OriginalMaxWalkSpeed;
}

// ============================================================================
// 慢走
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::EnterWalkMode()
{
	if (bIsSprinting) ExitSprintMode();
	if (!bIsWalking) OriginalMaxWalkSpeed = MaxWalkSpeed;
	bIsWalking = true;
	MaxWalkSpeed = WalkMaxWalkSpeed;
}

void UOpenWorldARPGCharacterMovementComponent::ExitWalkMode()
{
	if (!bIsWalking) return;
	bIsWalking = false;
	MaxWalkSpeed = OriginalMaxWalkSpeed;
}

// ============================================================================
// 瞄准
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::EnterAimMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (!bIsAiming)
	{
		OriginalMaxWalkSpeed = MaxWalkSpeed;
		bOriginalOrientRotationToMovement = bOrientRotationToMovement;
		bOriginalUseControllerRotationYaw = Char->bUseControllerRotationYaw;
	}

	bIsAiming = true;
	MaxWalkSpeed = AimMaxWalkSpeed;
	bOrientRotationToMovement = AimOrientRotationToMovement;
	Char->bUseControllerRotationYaw = AimUseControllerRotationYaw;
}

void UOpenWorldARPGCharacterMovementComponent::ExitAimMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (!bIsAiming) return;

	bIsAiming = false;
	MaxWalkSpeed = OriginalMaxWalkSpeed;
	bOrientRotationToMovement = bOriginalOrientRotationToMovement;
	Char->bUseControllerRotationYaw = bOriginalUseControllerRotationYaw;
}

// ============================================================================
// 下落
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::SetFallingRotationInterpSpeed(float InSpeed)
{
	FallingRotationInterpSpeed = InSpeed;
}
