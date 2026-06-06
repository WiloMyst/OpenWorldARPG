// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacterAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "Components/MovementStateMachineComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

UPlayerCharacterAnimInstance::UPlayerCharacterAnimInstance()
{
}

void UPlayerCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// ============================================================
	// 工作线程安全区域：只使用线程安全的数据访问方式
	// 不调用任何 UObject 函数、不访问 Delegate、不访问 GameplayTag
	// ============================================================

	// 通过 TryGetPawnOwner() 获取 Pawn 是线程安全的（FAnimInstanceProxy 内部缓存）
	APawn* OwningPawn = TryGetPawnOwner();
	if (!OwningPawn) return;

	// 1. 速度计算（仅访问 Velocity，线程安全）
	const FVector Velocity = OwningPawn->GetVelocity();
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.0f);
	ThreadSafe_GroundSpeed = Velocity2D.Size();
	ThreadSafe_bIsMoving = ThreadSafe_GroundSpeed > 3.0f;

	// 2. 下落状态（访问 UCharacterMovementComponent 的 MovementMode 是线程安全的只读属性）
	if (const ACharacter* Character = Cast<ACharacter>(OwningPawn))
	{
		if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			ThreadSafe_bIsFalling = MoveComp->IsFalling();

			// 3. 自定义移动模式检测（攀爬等）
			const uint8 CustomMode = static_cast<uint8>(MoveComp->CustomMovementMode);
			ThreadSafe_bIsClimbing = (MoveComp->MovementMode == MOVE_Custom && CustomMode == static_cast<uint8>(ECustomMovementMode::Climbing));
		}
	}

	// 3.5 FSM 状态检测 (墙角过渡等)
	// 注意：FindComponentByClass 不是线程安全的，不能在工作线程调用
	// 改为在 NativeUpdateAnimation（主线程）中查询，此处仅从 CMC 的 CustomMovementMode 推断
	ThreadSafe_bIsCornerTransition = false;
	ThreadSafe_CurrentMovementState = EMovementState::None;
	if (ThreadSafe_bIsClimbing)
	{
		ThreadSafe_CurrentMovementState = EMovementState::Climbing;
	}
	else if (ThreadSafe_bIsFalling)
	{
		ThreadSafe_CurrentMovementState = EMovementState::Falling;
	}
	else if (ThreadSafe_bIsMoving || ThreadSafe_GroundSpeed > 0.0f)
	{
		ThreadSafe_CurrentMovementState = EMovementState::Grounded;
	}

	// 4. 移动方向角计算（纯数学运算，线程安全）
	if (ThreadSafe_bIsMoving)
	{
		const FRotator Rotation = OwningPawn->GetActorRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 速度在角色局部坐标系下的投影
		const float ForwardDot = Velocity2D | ForwardDir;
		const float RightDot = Velocity2D | RightDir;

		ThreadSafe_MovementDirection = FMath::Atan2(RightDot, ForwardDot) * (180.0f / PI);
	}
	else
	{
		ThreadSafe_MovementDirection = 0.0f;
	}
}

void UPlayerCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// ============================================================
	// 主线程区域：从线程安全缓存拷贝到 UPROPERTY（供 AnimBP 读取）
	// 同时处理需要主线程上下文的逻辑
	// ============================================================

	GroundSpeed = ThreadSafe_GroundSpeed;
	bIsFalling = ThreadSafe_bIsFalling;
	bIsMoving = ThreadSafe_bIsMoving;
	MovementDirection = ThreadSafe_MovementDirection;
	bIsClimbing = ThreadSafe_bIsClimbing;
	CurrentMovementState = ThreadSafe_CurrentMovementState;

	// 墙角过渡状态需要在主线程查询 FSM（FindComponentByClass 不是线程安全的）
	bIsCornerTransition = false;
	if (const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner()))
	{
		if (const UMovementStateMachineComponent* FSM = Character->FindComponentByClass<UMovementStateMachineComponent>())
		{
			bIsCornerTransition = FSM->IsInCornerTransition();
			CurrentMovementState = FSM->GetCurrentState();
		}
	}

	// 瞄准状态需要访问 ASC（GameplayTag 查询必须在主线程）
	bIsAiming = false;
	if (const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner()))
	{
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				// 查询是否有瞄准 Tag（需在子类蓝图中配置 AimingStateTag）
				static const FGameplayTag AimingTag = FGameplayTag::RequestGameplayTag(FName("State.Aiming"));
				bIsAiming = ASC->HasMatchingGameplayTag(AimingTag);
			}
		}
	}
}
