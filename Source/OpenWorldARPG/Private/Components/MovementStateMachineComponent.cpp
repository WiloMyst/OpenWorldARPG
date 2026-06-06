// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/MovementStateMachineComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UMovementStateMachineComponent::UMovementStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMovementStateMachineComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		OwnerCharacter = Player;
		MovementComp = Player->GetCharacterMovement();
		CustomMovementComp = Player->GetCustomMovementComp();

		// 初始化状态：根据引擎 MovementMode 设置
		if (MovementComp)
		{
			if (MovementComp->IsWalking() || MovementComp->IsMovingOnGround())
			{
				CurrentState = EMovementState::Grounded;
			}
			else if (MovementComp->IsFalling())
			{
				CurrentState = EMovementState::Falling;
			}
			else
			{
				CurrentState = EMovementState::None;
			}
		}
	}
}

void UMovementStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MovementComp) return;

	// 墙角过渡状态由 ClimbingComponent 驱动，不做自动检测
	if (CurrentState == EMovementState::CornerTransition) return;

	// 攀爬状态由 ClimbingComponent 驱动进入/退出，但需要检测是否意外脱离
	if (CurrentState == EMovementState::Climbing)
	{
		// 如果引擎侧不再是 Custom/Climbing 或 Custom/ClimbingCornerTransition，说明被外部打断了
		if (MovementComp->MovementMode == MOVE_Custom)
		{
			ECustomMovementMode CurrCustom = static_cast<ECustomMovementMode>(MovementComp->CustomMovementMode);
			if (CurrCustom != ECustomMovementMode::Climbing && CurrCustom != ECustomMovementMode::ClimbingCornerTransition)
			{
				RequestStateChange(EMovementState::Falling);
			}
		}
		else
		{
			RequestStateChange(EMovementState::Falling);
		}
		return;
	}

	// 其他状态：根据引擎 MovementMode 同步
	DetectStateFromEngine();
}

bool UMovementStateMachineComponent::IsClimbing() const
{
	return CurrentState == EMovementState::Climbing || CurrentState == EMovementState::CornerTransition;
}

bool UMovementStateMachineComponent::IsFalling() const
{
	return CurrentState == EMovementState::Falling;
}

bool UMovementStateMachineComponent::IsGrounded() const
{
	return CurrentState == EMovementState::Grounded;
}

bool UMovementStateMachineComponent::IsInCornerTransition() const
{
	return CurrentState == EMovementState::CornerTransition;
}

bool UMovementStateMachineComponent::RequestStateChange(EMovementState NewState)
{
	if (NewState == CurrentState) return false;

	// 状态切换验证
	// 墙角过渡期间不允许外部打断 (除非是强制)
	if (CurrentState == EMovementState::CornerTransition) return false;

	EMovementState OldState = CurrentState;

	OnExitState(OldState);
	CurrentState = NewState;
	OnEnterState(NewState);

	OnStateChanged.Broadcast(OldState, NewState);
	return true;
}

void UMovementStateMachineComponent::ForceStateChange(EMovementState NewState)
{
	if (NewState == CurrentState) return;

	EMovementState OldState = CurrentState;
	OnExitState(OldState);
	CurrentState = NewState;
	OnEnterState(NewState);
	OnStateChanged.Broadcast(OldState, NewState);
}

void UMovementStateMachineComponent::OnEnterState(EMovementState State)
{
	switch (State)
	{
	case EMovementState::Grounded:
		if (MovementComp)
		{
			MovementComp->bOrientRotationToMovement = true;
			MovementComp->RotationRate = FRotator(0.0f, StateConfigs.GroundedRotationRate, 0.0f);
			MovementComp->AirControl = 0.0f; // 地面不需要空中控制力
		}
		break;

	case EMovementState::Falling:
		if (MovementComp)
		{
			MovementComp->bOrientRotationToMovement = true;
			MovementComp->RotationRate = FRotator(0.0f, StateConfigs.FallingConfig.RotationInterpSpeed * 100.0f, 0.0f);
			MovementComp->AirControl = StateConfigs.FallingConfig.AirControl;
		}
		if (CustomMovementComp)
		{
			CustomMovementComp->SetFallingRotationInterpSpeed(StateConfigs.FallingConfig.RotationInterpSpeed);
		}
		break;

	case EMovementState::Climbing:
		if (MovementComp)
		{
			MovementComp->bOrientRotationToMovement = false;
			MovementComp->AirControl = 0.0f;
		}
		break;

	case EMovementState::CornerTransition:
		// 墙角过渡期间冻结常规移动输入
		if (MovementComp)
		{
			MovementComp->bOrientRotationToMovement = false;
		}
		break;

	default:
		break;
	}
}

void UMovementStateMachineComponent::OnExitState(EMovementState State)
{
	switch (State)
	{
	case EMovementState::Climbing:
		if (CustomMovementComp)
		{
			CustomMovementComp->ClearClimbState();
		}
		break;

	case EMovementState::CornerTransition:
		if (CustomMovementComp)
		{
			CustomMovementComp->ClearCornerTransition();
		}
		break;

	default:
		break;
	}
}

void UMovementStateMachineComponent::DetectStateFromEngine()
{
	if (!MovementComp) return;

	uint8 CurrentEngineMode = MovementComp->MovementMode;

	// 检测引擎侧状态变化
	if (CurrentEngineMode != PrevEngineMovementMode)
	{
		PrevEngineMovementMode = CurrentEngineMode;

		switch (CurrentEngineMode)
		{
		case MOVE_Walking:
		case MOVE_NavWalking:
			RequestStateChange(EMovementState::Grounded);
			break;

		case MOVE_Falling:
			RequestStateChange(EMovementState::Falling);
			break;

		default:
			break;
		}
	}
}

void UMovementStateMachineComponent::ApplyStateParamsToCMC()
{
	if (!MovementComp) return;

	switch (CurrentState)
	{
	case EMovementState::Grounded:
		MovementComp->AirControl = 0.0f;
		MovementComp->RotationRate = FRotator(0.0f, StateConfigs.GroundedRotationRate, 0.0f);
		break;

	case EMovementState::Falling:
		MovementComp->AirControl = StateConfigs.FallingConfig.AirControl;
		MovementComp->RotationRate = FRotator(0.0f, StateConfigs.FallingConfig.RotationInterpSpeed * 100.0f, 0.0f);
		break;

	default:
		break;
	}
}
