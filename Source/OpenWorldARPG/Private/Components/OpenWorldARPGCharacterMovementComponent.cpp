// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

UOpenWorldARPGCharacterMovementComponent::UOpenWorldARPGCharacterMovementComponent()
{
}

void UOpenWorldARPGCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// 根据 CustomMovementMode 分发到对应的物理函数
	switch (static_cast<ECustomMovementMode>(CustomMovementMode))
	{
	case ECustomMovementMode::Climbing:
		PhysClimbing(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Gliding:
		// TODO: 滑翔物理
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	case ECustomMovementMode::Swimming:
		// TODO: 游泳物理 (如果需要覆盖默认游泳)
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	default:
		Super::PhysCustom(DeltaTime, Iterations);
		break;
	}
}

void UOpenWorldARPGCharacterMovementComponent::SetClimbInput(float InInputRight, float InInputUp)
{
	ClimbInputRight = InInputRight;
	ClimbInputUp = InInputUp;
}

void UOpenWorldARPGCharacterMovementComponent::SetClimbWallNormal(const FVector& InNormal)
{
	ClimbWallNormal = InNormal;
}

void UOpenWorldARPGCharacterMovementComponent::ClearClimbInput()
{
	ClimbInputRight = 0.0f;
	ClimbInputUp = 0.0f;
	ClimbWallNormal = FVector::ZeroVector;
}

void UOpenWorldARPGCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		if (ClimbGravityScale > 0.0f)
		{
			// 有重力时施加下坠 (体力耗尽场景)
			Velocity.Z += GetGravityZ() * ClimbGravityScale * DeltaTime;
		}
		else
		{
			// 无重力：速度清零，完全由输入驱动
			Velocity = FVector::ZeroVector;
		}

		// 根据输入计算期望速度
		if (!FMath::IsNearlyZero(ClimbInputRight) || !FMath::IsNearlyZero(ClimbInputUp))
		{
			ACharacter* Char = Cast<ACharacter>(GetOwner());
			if (Char)
			{
				FVector MoveDir = (Char->GetActorRightVector() * ClimbInputRight
					+ Char->GetActorUpVector() * ClimbInputUp).GetSafeNormal();
				Velocity = MoveDir * ClimbMoveSpeed;
			}
		}
	}

	// 标准移动流程：应用速度 + 碰撞滑墙
	ApplyRootMotionToVelocity(DeltaTime);
	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FStepDownResult StepDownResult;

	// MoveUpdatedComponent 会处理碰撞和滑墙 (slide)
	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit, ETeleportType::None);

	// 碰撞后处理
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Velocity.GetSafeNormal());

		// 滑墙：沿碰撞面滑动
		SlideAlongSurface(Velocity * DeltaTime, 1.0f - Hit.Time, Hit.Normal, Hit, true);

		// 更新墙壁法线 (贴墙移动时法线会变化)
		if (Hit.bStartPenetrating == false)
		{
			ClimbWallNormal = Hit.Normal;
		}
	}

	// 更新朝向：始终面向墙壁
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (!ClimbWallNormal.IsNearlyZero())
		{
			FRotator TargetRot = FRotationMatrix::MakeFromX(-ClimbWallNormal).Rotator();
			FRotator NewRot = FMath::RInterpTo(Char->GetActorRotation(), TargetRot, DeltaTime, ClimbRotationInterpSpeed);
			Char->SetActorRotation(NewRot);
		}
	}

	// 更新最终位置
	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasActiveRootMotionSources())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}
