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

// ============================================================================
// 构造函数 & 初始化
// ============================================================================

UOpenWorldARPGCharacterMovementComponent::UOpenWorldARPGCharacterMovementComponent()
{
}

void UOpenWorldARPGCharacterMovementComponent::CacheOwnerReferences()
{
	CachedOwnerCharacter = Cast<ACharacter>(GetOwner());
	if (CachedOwnerCharacter)
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedOwnerCharacter);
	}
}

void UOpenWorldARPGCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存 GravityScale 和 AirControl 的默认值，供非对称重力逻辑恢复使用
	DefaultGravityScale = GravityScale;
	DefaultAirControl = AirControl;
}

// ============================================================================
// 引擎重写 - 生命周期
//
// 引擎执行管线：
//   TickComponent
//     → UpdateCharacterStateBeforeMovement  ← 状态检测（Falling→Climbing 等）
//     → PerformMovement
//         → PhysStep → PhysCustom           ← 物理模拟
//     → UpdateCharacterStateAfterMovement
//   OnMovementModeChanged                    ← 模式切换回调（Tag/参数恢复）
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	// ==========================================
	// 1. Falling 状态：检测前方墙壁，尝试进入攀爬
	//    条件：有输入方向 + 前方有可攀爬墙壁 + 输入朝向墙壁
	// ==========================================
	if (MovementMode == MOVE_Falling)
	{
		// ==========================================
		// 非对称重力：基于 Velocity.Z 划分上升/下落阶段
		// 上升阶段：正常重力 + 允许空中控制与转身
		// 下落阶段：重力倍率增加 + 剥夺空中控制 + 锁死朝向
		// ==========================================
		if (Velocity.Z > 0.0f)
		{
			// 上升阶段
			GravityScale = RisingGravityScale;
			AirControl = RisingAirControl;
			RotationRate = FRotator(0.0f, RisingRotationRate, 0.0f);
			bOrientRotationToMovement = true;
		}
		else
		{
			// 下落阶段
			GravityScale = FallingGravityScale;
			AirControl = FallingAirControl;
			bOrientRotationToMovement = false;
		}

		CheckFallingToClimb();
	}

	// ==========================================
	// 1b. Walking 状态：地面走墙时检测前方可攀爬墙壁
	//     与空中检测逻辑相同：检测到可攀爬墙壁 → 发送 TryClimb Event
	// ==========================================
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		CheckGroundedToClimb();
	}

	// ==========================================
	// 2. Climbing 状态：执行攀爬期间的持续检测
	//    顺序：落地检测 > 墙角检测 > 法线更新 > 翻越检测
	//    每一步都可能改变当前模式，所以需要检查是否仍在 Climbing
	// ==========================================
	if (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Climbing)
	{
		// 2a. 攀爬转地面（优先级最高：已经到地面了就不需要其他检测）
		CheckClimbToGround();

		// 如果已经切换到 Walking，跳过后续检测
		if (MovementMode != MOVE_Custom) return;

		// 2b. 墙角过渡检测
		CheckCornerTransition();

		// 如果已经切换到 CornerTransition，跳过后续检测
		if (MovementMode != MOVE_Custom
			|| static_cast<ECustomMovementMode>(CustomMovementMode) != ECustomMovementMode::Climbing) return;

		// 2c. 更新墙面法线（确保角色始终贴合墙壁）
		FHitResult ChestHit, HeadHit;
		if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
		{
			ClimbWallNormal = ChestHit.Normal;
		}

		// 2d. 翻越检测（优先级最低：只有其他条件都满足时才检测）
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
	// ==========================================
	// 离开自定义模式时恢复物理参数
	// 无论是因为 GA 调用 Exit，还是因为引擎自动切换（如着地），
	// 都必须恢复原始物理参数，防止 GravityScale 等永久丢失
	// ==========================================
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

	// 非对称重力恢复：如果之前是下落状态，且现在不再是下落状态（比如落地、攀爬等），恢复默认参数
	bool bWasFalling = (PreviousMovementMode == MOVE_Falling);
	bool bIsStillFalling = (MovementMode == MOVE_Falling);
	if (bWasFalling && !bIsStillFalling)
	{
		GravityScale = DefaultGravityScale;
		AirControl = DefaultAirControl;
		bOrientRotationToMovement = true;
		// RotationRate 的恢复由下方 Walking 状态分支覆盖，此处无需重复处理
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// ==========================================
	// GAS Tag 映射：CMC 管理被动运动状态 Tag
	// ==========================================
	if (CachedASC)
	{
		// 1. 清除被动运动状态 Tags
		if (AirborneTag.IsValid())  CachedASC->RemoveLooseGameplayTag(AirborneTag);
		if (FallingTag.IsValid())   CachedASC->RemoveLooseGameplayTag(FallingTag);
		if (SwimmingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(SwimmingTag);
		if (FastSwimmingTag.IsValid()) CachedASC->RemoveLooseGameplayTag(FastSwimmingTag);

		// 2. 根据 MovementMode 注入当前被动状态 Tag
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
		{
			AirControl = 0.0f;
			RotationRate = GroundedRotationRate;
			bOrientRotationToMovement = true;

			// 落地时记录安全位置
			UpdateLastSafeLocation();

			// 从空中落地时发送停止滑翔事件
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

	// 广播事件
	OnMovementModeChangedDelegate.Broadcast(PreviousMovementMode, MovementMode.GetValue(), PreviousCustomMode, CustomMovementMode);
}

// ============================================================================
// 通用状态查询
// ============================================================================

bool UOpenWorldARPGCharacterMovementComponent::IsFalling() const { return MovementMode == MOVE_Falling; }
bool UOpenWorldARPGCharacterMovementComponent::IsGrounded() const { return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking; }

// ============================================================================
// 攀爬模块
//
// 组织顺序：公开接口 → 状态检测 → 状态转换 → 物理模拟 → 辅助
// ============================================================================

// ------------------------------------
// 攀爬 - 公开接口
// ------------------------------------

void UOpenWorldARPGCharacterMovementComponent::TryClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	// 只在地面或下落状态下允许尝试攀爬
	if (!IsGrounded() && !IsFalling()) return;

	FHitResult ChestHit;
	if (DetectClimbableWall(ChestHit))
	{
		// 检测到可攀爬墙壁，发送 Event 让 GA 决定是否进入攀爬
		if (TryClimbEventTag.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = TryClimbEventTag;
			Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(ChestHit);
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Char, TryClimbEventTag, Payload);
		}
	}
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
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	if (!IsClimbing()) return;

	CheckAndClimbUp();
}

void UOpenWorldARPGCharacterMovementComponent::FinishClimbUp()
{
	if (!bIsClimbingUp) return;

	bIsClimbingUp = false;
	ClearClimbState();

	// 恢复到下落模式
	SetMovementMode(MOVE_Falling);
	bOrientRotationToMovement = true;

	ACharacter* Char = CachedOwnerCharacter.Get();
	if (Char)
	{
		FRotator CurrentRot = Char->GetActorRotation();
		Char->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
	}
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

// ------------------------------------
// 攀爬 - 状态检测 (由 UpdateCharacterStateBeforeMovement 调用)
// ------------------------------------

void UOpenWorldARPGCharacterMovementComponent::CheckFallingToClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	// 使用 CMC 内部的 Acceleration 判断输入意图
	if (Acceleration.IsNearlyZero()) return;

	FHitResult ChestHit, HeadHit;
	if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
	{
		if (ChestHit.GetActor() && ChestHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return;
		if (!IsWallClimbable(ChestHit.Normal)) return;

		float DotResult = FVector::DotProduct(Acceleration, ChestHit.Normal);
		if (DotResult < MinInputDotProduct)
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
}

void UOpenWorldARPGCharacterMovementComponent::CheckGroundedToClimb()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	// 地面走墙检测：与空中检测逻辑相同，复用 DetectClimbableWall
	// 检测到可攀爬墙壁 → 发送 TryClimb Event → GA 决定是否进入攀爬
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
		// 计算角色双脚离地高度
		const float FeetHeightAboveGround = HitResult.Distance - CapsuleHalfHeight;

		// 只有双脚离地超过最小高度时才触发攀爬转地面
		// 防止在墙根处刚进入攀爬就被判定为"已到地面"而退出，导致鬼畜
		if (FeetHeightAboveGround < MinClimbHeightAboveGround)
		{
			return;
		}
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

	// 使用 CMC 内部的 Acceleration 获取横向输入方向
	if (Acceleration.IsNearlyZero()) return;

	// 计算横向输入分量（角色右方向上的投影）
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

	// 翻越空间检测：在角色前方偏上位置检测是否有足够空间
	const FVector StartLoc = Char->GetActorLocation()
		+ (Char->GetActorForwardVector() * ClimbPredictOffset)
		+ FVector(0.0f, 0.0f, ClimbUpCheckHalfHeight);

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(ClimbUpCheckRadius, ClimbUpCheckHalfHeight);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);

	FHitResult HitResult;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult, StartLoc, StartLoc, FQuat::Identity, ClimbTraceChannel, CapsuleShape, Params);

	// 没有碰撞 = 前方有足够空间可以翻越
	if (!bHit)
	{
		DoClimbUp();
	}
}

// ------------------------------------
// 攀爬 - 状态转换
// ------------------------------------

void UOpenWorldARPGCharacterMovementComponent::EnterClimb(const FHitResult& WallHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	ClimbWallNormal = WallHit.Normal;

	// 1. 先清零速度，再切换到攀爬模式
	//    如果先 SetMovementMode 再清零，OnMovementModeChanged 可能在零速度前被触发
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
	bOrientRotationToMovement = false;

	// 2. 计算吸附目标
	const FRotator TargetRot = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);
	const FRotator FinalRot = FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.0f);
	const FVector TargetLoc = Char->GetActorLocation() + FVector(0.0f, 0.0f, WallSnapZOffset);
	SetClimbSnapTarget(TargetLoc, FinalRot, WallSnapTime);
}

void UOpenWorldARPGCharacterMovementComponent::DoClimbUp()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return;

	bIsClimbingUp = true;

	// 计算翻越目标位置（基于角色朝向 + ClimbUpOffset）
	const FVector TargetLoc = Char->GetActorLocation()
		+ Char->GetActorForwardVector() * ClimbUpOffset.X
		+ Char->GetActorRightVector() * ClimbUpOffset.Y
		+ FVector(0.0f, 0.0f, ClimbUpOffset.Z);
	const FRotator TargetRot = FRotator(0.0f, Char->GetActorRotation().Yaw, 0.0f);

	// 设置翻越目标并切换到 ClimbUp 模式
	ClimbUpTargetLocation = TargetLoc;
	ClimbUpTargetRotation = TargetRot;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbUp));

	// 通过 Delegate 请求 Character 播放翻越蒙太奇
	// CMC 只管物理位移，动画播放由 Character 负责
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

// ------------------------------------
// 攀爬 - 物理模拟
// ------------------------------------

void UOpenWorldARPGCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 墙面吸附阶段：插值到目标位置后结束吸附
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

		// 吸附到达阈值后结束
		const float DistSq = FVector::DistSquared(Char->GetActorLocation(), SnapTargetLocation);
		if (DistSq < 4.0f)
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

		// 使用 CMC 内部的 Acceleration 获取输入
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

	// 朝向：始终面向墙壁
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

	// 墙角过渡期间：零速度，完全由插值驱动
	Velocity = FVector::ZeroVector;

	// 位置插值
	const float PosInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexPositionInterpSpeed : ConcavePositionInterpSpeed;
	const FVector CurrentLocation = Char->GetActorLocation();
	const FVector NewLocation = FMath::VInterpTo(CurrentLocation, CornerTargetLocation, DeltaTime, PosInterpSpeed);

	// 旋转插值：面向目标法线的反方向
	const float RotInterpSpeed = (CornerType == ECornerType::Convex) ? ConvexRotationInterpSpeed : ConcaveRotationInterpSpeed;
	const FRotator TargetRot = FRotationMatrix::MakeFromX(-CornerTargetNormal).Rotator();
	const FRotator CurrentRot = Char->GetActorRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, RotInterpSpeed);

	// 应用位移
	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 检测过渡是否完成
	const float DistSq = FVector::DistSquared(Char->GetActorLocation(), CornerTargetLocation);
	const bool bRotArrived = NewRot.Equals(TargetRot, CornerRotationArrivalThreshold);

	if (DistSq < FMath::Square(CornerArrivalThreshold) && bRotArrived)
	{
		// 过渡完成：恢复 Climbing 模式
		CornerTargetLocation = FVector::ZeroVector;
		CornerTargetNormal = FVector::ZeroVector;
		CornerType = ECornerType::None;
		ClimbWallNormal = CornerTargetNormal; // 使用新墙面法线
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

	// 翻越期间：零速度，完全由插值驱动
	Velocity = FVector::ZeroVector;

	// 位置插值（使用恒速插值，保证翻越时间稳定）
	const FVector CurrentLocation = Char->GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, ClimbUpTargetLocation, DeltaTime, ClimbUpPositionInterpSpeed);

	// 旋转插值
	const FRotator CurrentRot = Char->GetActorRotation();
	const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, ClimbUpTargetRotation, DeltaTime, ClimbUpRotationInterpSpeed);

	// 应用位移
	FHitResult Hit;
	SafeMoveUpdatedComponent(NewLocation - CurrentLocation, NewRot.Quaternion(), true, Hit, ETeleportType::None);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(NewLocation - CurrentLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 翻越到达目标后，等待 Character 的蒙太奇回调来调用 FinishClimbUp
	// CMC 不主动切换模式，只负责物理位移
}

// ------------------------------------
// 攀爬 - 辅助
// ------------------------------------

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

	ACharacter* Char = CachedOwnerCharacter.Get();
	if (Char)
	{
		TargetLoc.Z = Char->GetActorLocation().Z;
	}

	return TargetLoc;
}

FVector UOpenWorldARPGCharacterMovementComponent::CalculateConcaveTargetLocation(
	const FVector& NewWallHitLocation, const FVector& NewWallNormal) const
{
	FVector TargetLoc = NewWallHitLocation + NewWallNormal * ConcaveSnapDistance;

	ACharacter* Char = CachedOwnerCharacter.Get();
	if (Char)
	{
		TargetLoc.Z = Char->GetActorLocation().Z;
	}

	return TargetLoc;
}

bool UOpenWorldARPGCharacterMovementComponent::DetectClimbableWall(FHitResult& OutChestHit)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char) return false;

	// 使用 CMC 内部的 Acceleration 判断输入意图
	if (Acceleration.IsNearlyZero()) return false;

	FHitResult HeadHit;
	if (PerformClimbTraces(FVector::ZeroVector, OutChestHit, HeadHit))
	{
		if (OutChestHit.GetActor() && OutChestHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return false;
		if (!IsWallClimbable(OutChestHit.Normal)) return false;

		// 检查输入方向是否朝向墙壁
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

	// 目标位置：墙面击中点沿法线方向偏移 TargetWallDistance，保持与墙面的安全距离
	// Z 轴保持角色当前高度（上墙动画会自带 Z 位移）
	const FVector TargetLocation = WallHit.Location + WallHit.Normal * TargetWallDistance;

	// 目标旋转：面朝墙壁反法线方向
	const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);

	return FTransform(TargetRotation, TargetLocation, FVector::OneVector);
}

void UOpenWorldARPGCharacterMovementComponent::ClearClimbState()
{
	ClimbWallNormal = FVector::ZeroVector;
	bIsSnappingToWall = false;
	bIsClimbingUp = false;
}

void UOpenWorldARPGCharacterMovementComponent::SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime)
{
	bIsSnappingToWall = true;
	SnapTargetLocation = InTargetLocation;
	SnapTargetRotation = InTargetRotation;
	// 从期望吸附时间反算插值速率：VInterpTo 在 time=T 时到达约 63%
	// 要在 SnapTime 内完成吸附，需要 InterpSpeed ≈ 3/SnapTime
	SnapInterpSpeed = (InSnapTime > 0.0f) ? (3.0f / InSnapTime) : 30.0f;
}

// ============================================================================
// 滑翔模块
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

bool UOpenWorldARPGCharacterMovementComponent::IsGliding() const
{
	return MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding;
}

float UOpenWorldARPGCharacterMovementComponent::GetDistanceToGround() const
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return -1.0f;
	}

	// 从角色脚底向下射线检测地面距离
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
		// 重力：即使 GlideGravityScale=0，也需手动施加微量重力模拟滑翔下沉感
		Velocity.Z += GetGravityZ() * 0.1f * DeltaTime;

		// 水平速度限制
		FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
		if (HorizontalVel.SizeSquared() > FMath::Square(GlideMaxHorizontalSpeed))
		{
			HorizontalVel = HorizontalVel.GetSafeNormal() * GlideMaxHorizontalSpeed;
		}

		// 垂直速度限制
		Velocity.Z = FMath::Clamp(Velocity.Z, GlideMinDescentSpeed, GlideMaxDescentSpeed);

		Velocity = FVector(HorizontalVel.X, HorizontalVel.Y, Velocity.Z);

		// 输入处理：使用归一化方向 + 合理加速值
		// Acceleration 是原始输入向量，模长等于 MaxAcceleration（默认 2048），
		// 不能直接乘以 AirControl * 100，否则速度暴增。
		// 正确做法：取方向，用固定加速值叠加。
		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero())
		{
			const FVector InputDir = InputVector.GetSafeNormal();
			const float GlideAcceleration = 800.0f; // 滑翔水平加速度 (cm/s²)
			Velocity += FVector(InputDir.X, InputDir.Y, 0.0f) * GlideAcceleration * DeltaTime;
		}

		// 水平阻力：防止无限加速，模拟空气阻力
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

	// 朝向：面向移动方向
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
// 游泳模块
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

bool UOpenWorldARPGCharacterMovementComponent::IsSwimming() const
{
	return MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Swimming;
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

bool UOpenWorldARPGCharacterMovementComponent::IsFastSwimming() const { return bIsFastSwimming; }

void UOpenWorldARPGCharacterMovementComponent::PhysSwimming(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 出水检测
	if (!IsStillInWater())
	{
		ExitSwimMode();
		return;
	}

	// 水面吸附
	ApplySurfaceSnapping(DeltaTime);

	// 水平移动处理
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
	// 框架预留：如果项目集成了 Water Plugin，在此处获取精确水面高度
	// float WaterSurfaceZ = UWaterLibrary::GetWaterSurfaceZ(GetWorld(), Char->GetActorLocation());
	// float TargetZ = WaterSurfaceZ + SurfaceSnapOffset;
	// FVector TargetLocation = Char->GetActorLocation();
	// TargetLocation.Z = FMath::FInterpTo(TargetLocation.Z, TargetZ, DeltaTime, SurfaceSnapInterpSpeed);
	// Char->SetActorLocation(TargetLocation);
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
// 冲刺模块
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

bool UOpenWorldARPGCharacterMovementComponent::IsSprinting() const { return bIsSprinting; }

// ============================================================================
// 慢走模块
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

bool UOpenWorldARPGCharacterMovementComponent::IsWalking() const { return bIsWalking; }

// ============================================================================
// 瞄准模块
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

bool UOpenWorldARPGCharacterMovementComponent::IsAiming() const { return bIsAiming; }

// ============================================================================
// 下落状态
// ============================================================================

void UOpenWorldARPGCharacterMovementComponent::SetFallingRotationInterpSpeed(float InSpeed)
{
	FallingRotationInterpSpeed = InSpeed;
}
