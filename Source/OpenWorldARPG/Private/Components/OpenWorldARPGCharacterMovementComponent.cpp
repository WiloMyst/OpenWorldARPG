// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/ClimbingComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
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
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedOwnerCharacter);
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
		PhysGliding(DeltaTime, Iterations);
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

void UOpenWorldARPGCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	float OldGravityScale = GravityScale;

	UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged START: Mode %d->%d, Custom %d->%d, GravityScale=%.2f"),
		(int32)PreviousMovementMode, (int32)MovementMode.GetValue(),
		(int32)PreviousCustomMode, (int32)CustomMovementMode,
		OldGravityScale);

	// ==========================================
	// 离开滑翔模式时恢复物理参数 (关键修复)
	// 无论是因为 GA 调用 ExitGlideMode，还是因为引擎自动切换（如着地），
	// 都必须恢复原始物理参数，防止 GravityScale 等永久丢失
	// ==========================================
	bool bWasGliding = (PreviousMovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(PreviousCustomMode) == ECustomMovementMode::Gliding);
	bool bIsStillGliding = (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding);

	if (bWasGliding && !bIsStillGliding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CMC] Leaving Gliding mode, restoring physics: GravityScale=%.2f->%.2f, AirControl=%.2f->%.2f"),
			GravityScale, OriginalGravityScale, AirControl, OriginalAirControl);
		GravityScale = OriginalGravityScale;
		AirControl = OriginalAirControl;
		RotationRate = OriginalRotationRate;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (!CachedASC) return;

	// ==========================================
	// 状态 1：落地 (Walking / NavWalking)
	// ==========================================
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged: LANDED (Walking)"));

		// 暴力移除所有空中相关标签
		if (AirborneTag.IsValid()) CachedASC->RemoveLooseGameplayTag(AirborneTag);
		if (FallingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(FallingTag);
		if (GlidingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(GlidingTag);

		// 设置 Walking 物理参数 (CMC 统一管理)
		AirControl = 0.0f;
		RotationRate = GroundedRotationRate;
		bOrientRotationToMovement = true;

		// 如果上一个状态是在空中，说明这是"刚刚落地"的一瞬间
		if (PreviousMovementMode == MOVE_Falling || PreviousMovementMode == MOVE_Custom)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged: Was airborne, sending StopGlideEvent"));
			// 完美复刻原 GA 的兜底逻辑：落地强制关闭滑翔伞
			if (StopGlideEventTag.IsValid() && CachedOwnerCharacter)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CachedOwnerCharacter.Get(), StopGlideEventTag, FGameplayEventData());
			}
		}
	}
	// ==========================================
	// 状态 2：自由掉落 (Falling)
	// ==========================================
	else if (MovementMode == MOVE_Falling)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged: FALLING, GravityScale=%.2f"), GravityScale);
		// 注入空中和掉落标签，移除滑翔标签
		if (AirborneTag.IsValid()) CachedASC->AddLooseGameplayTag(AirborneTag);
		if (FallingTag.IsValid())  CachedASC->AddLooseGameplayTag(FallingTag);
		if (GlidingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(GlidingTag);

		// 设置 Falling 物理参数 (CMC 统一管理)
		AirControl = FallingAirControl;
		RotationRate = FallingRotationRate;
		bOrientRotationToMovement = true;
	}
	// ==========================================
	// 状态 3：特殊空中模式 (例如滑翔 Gliding)
	// ==========================================
	else if (MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged: GLIDING, GravityScale=%.2f, AirControl=%.2f"), GravityScale, AirControl);
		// 注入空中和滑翔标签，移除掉落标签
		if (AirborneTag.IsValid()) CachedASC->AddLooseGameplayTag(AirborneTag);
		if (GlidingTag.IsValid())  CachedASC->AddLooseGameplayTag(GlidingTag);
		if (FallingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(FallingTag);
	}

	UE_LOG(LogTemp, Warning, TEXT("[CMC] OnMovementModeChanged END: GravityScale=%.2f, AirControl=%.2f"), GravityScale, AirControl);
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

// ==========================================
// 滑翔接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::EnterGlideMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CMC] EnterGlideMode: BEFORE GravityScale=%.2f, AirControl=%.2f, RotationRate=%s"),
		GravityScale, AirControl, *RotationRate.ToString());

	// 1. 缓存原始物理参数 (在修改之前)
	OriginalGravityScale = GravityScale;
	OriginalAirControl = AirControl;
	OriginalRotationRate = RotationRate;

	// 2. 应用滑翔物理参数 (CMC 自己的 UPROPERTY 配置，不依赖 GA 传入)
	GravityScale = GlideGravityScale;
	AirControl = GlideAirControl;
	RotationRate = GlideRotationRate;
	bOrientRotationToMovement = true;

	UE_LOG(LogTemp, Warning, TEXT("[CMC] EnterGlideMode: AFTER GravityScale=%.2f(Orig=%.2f), AirControl=%.2f(Orig=%.2f), LaunchVel=%s"),
		GravityScale, OriginalGravityScale, AirControl, OriginalAirControl, *GlideLaunchVelocity.ToString());

	// 3. 施加初始弹射力 (使用 CMC 自己的 GlideLaunchVelocity)
	Launch(GlideLaunchVelocity);

	// 4. 切换到滑翔自定义移动模式
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Gliding));
}

void UOpenWorldARPGCharacterMovementComponent::ExitGlideMode()
{
	UE_LOG(LogTemp, Warning, TEXT("[CMC] ExitGlideMode: GravityScale=%.2f, AirControl=%.2f"),
		GravityScale, AirControl);

	// 物理参数恢复由 OnMovementModeChanged 统一处理
	// (当 SetMovementMode 切换离开 Custom/Gliding 时自动触发恢复)
	// 这里只负责切换运动模式
	SetMovementMode(MOVE_Falling);
}

bool UOpenWorldARPGCharacterMovementComponent::IsGliding() const
{
	return MovementMode == MOVE_Custom && static_cast<ECustomMovementMode>(CustomMovementMode) == ECustomMovementMode::Gliding;
}

// ==========================================
// 冲刺接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::EnterSprintMode()
{
	// 如果正在慢走，先退出慢走
	if (bIsWalking)
	{
		ExitWalkMode();
	}

	// 缓存原始速度 (仅在非冲刺状态下缓存，避免覆盖)
	if (!bIsSprinting)
	{
		OriginalMaxWalkSpeed = MaxWalkSpeed;
	}

	bIsSprinting = true;
	MaxWalkSpeed = SprintMaxWalkSpeed;
}

void UOpenWorldARPGCharacterMovementComponent::ExitSprintMode()
{
	if (!bIsSprinting) return;

	bIsSprinting = false;
	MaxWalkSpeed = OriginalMaxWalkSpeed;
}

bool UOpenWorldARPGCharacterMovementComponent::IsSprinting() const
{
	return bIsSprinting;
}

// ==========================================
// 慢走接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::EnterWalkMode()
{
	// 如果正在冲刺，先退出冲刺
	if (bIsSprinting)
	{
		ExitSprintMode();
	}

	// 缓存原始速度 (仅在非慢走状态下缓存，避免覆盖)
	if (!bIsWalking)
	{
		OriginalMaxWalkSpeed = MaxWalkSpeed;
	}

	bIsWalking = true;
	MaxWalkSpeed = WalkMaxWalkSpeed;
}

void UOpenWorldARPGCharacterMovementComponent::ExitWalkMode()
{
	if (!bIsWalking) return;

	bIsWalking = false;
	MaxWalkSpeed = OriginalMaxWalkSpeed;
}

bool UOpenWorldARPGCharacterMovementComponent::IsWalking() const
{
	return bIsWalking;
}

// ==========================================
// 瞄准接口实现
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::EnterAimMode()
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	// 缓存原始参数 (仅在非瞄准状态下缓存，避免覆盖)
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

bool UOpenWorldARPGCharacterMovementComponent::IsAiming() const
{
	return bIsAiming;
}

// ==========================================
// 滑翔物理模拟
// ==========================================

void UOpenWorldARPGCharacterMovementComponent::PhysGliding(float DeltaTime, int32 Iterations)
{
	ACharacter* Char = CachedOwnerCharacter.Get();
	if (!Char)
	{
		Char = Cast<ACharacter>(GetOwner());
		if (!Char) return;
	}

	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// ==========================================
	// 1. 应用重力
	// ==========================================
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		// 滑翔使用自定义重力 (GravityScale 已在 EnterGlideMode 中设置)
		Velocity.Z += GetGravityZ() * DeltaTime;

		// 限制垂直速度在 [MinDescentSpeed, MaxDescentSpeed] 范围内
		Velocity.Z = FMath::Clamp(Velocity.Z, GlideMinDescentSpeed, GlideMaxDescentSpeed);
	}

	// ==========================================
	// 2. 处理水平输入 (使用引擎管线中的 Acceleration)
	// ==========================================
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		// 空中控制力已在 EnterGlideMode 中设置
		// 引擎的 AirControl 机制会在 PhysFalling 中处理水平加速
		// 这里我们手动处理，因为我们在 Custom 模式
		FVector InputVector = Acceleration;
		if (!InputVector.IsNearlyZero())
		{
			// 将输入投影到水平面
			FVector HorizontalInput = FVector(InputVector.X, InputVector.Y, 0.0f);
			if (!HorizontalInput.IsNearlyZero())
			{
				float InputMag = HorizontalInput.Size();
				HorizontalInput.Normalize();

				// 空中控制力决定角色跟随输入转向的程度
				float CurrentSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
				float Accelerate = AirControl * InputMag * DeltaTime * 1000.0f;
				FVector HorizontalVelocity = FVector(Velocity.X, Velocity.Y, 0.0f) + HorizontalInput * Accelerate;

				// 限制水平速度
				if (HorizontalVelocity.Size() > GlideMaxHorizontalSpeed)
				{
					HorizontalVelocity = HorizontalVelocity.GetSafeNormal() * GlideMaxHorizontalSpeed;
				}

				Velocity.X = HorizontalVelocity.X;
				Velocity.Y = HorizontalVelocity.Y;
			}
		}
	}

	// ==========================================
	// 3. 应用 Root Motion
	// ==========================================
	ApplyRootMotionToVelocity(DeltaTime);

	// ==========================================
	// 4. 执行移动
	// ==========================================
	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	// ==========================================
	// 5. 碰撞处理
	// ==========================================
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// ==========================================
	// 6. 着地检测 (滑翔中碰到地面自动退出)
	// ==========================================
	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	// Custom 模式下引擎不会自动检测着地，需要手动 FindFloor
	// 如果角色下方有地面，切换到 Walking（由 OnMovementModeChanged 处理 Tag 清理和 StopGlideEvent）
	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
	if (FloorResult.IsWalkableFloor())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CMC] PhysGliding: detected walkable floor, switching to Walking"));
		SetMovementMode(MOVE_Walking);
	}
}
