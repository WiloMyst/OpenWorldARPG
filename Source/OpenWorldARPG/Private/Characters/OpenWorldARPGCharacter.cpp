// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/OpenWorldARPGCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AOpenWorldARPGCharacter::AOpenWorldARPGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	TeamId = FGenericTeamId(10);
}

void AOpenWorldARPGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bIsDead);
}

void AOpenWorldARPGCharacter::HandleDeath_Implementation()
{
	if (bIsDead) return;
	bIsDead = true;
	ApplyDeathState();
}

void AOpenWorldARPGCharacter::HandleRevive_Implementation()
{
	bIsDead = false;
	ApplyReviveState();
}

void AOpenWorldARPGCharacter::OnRep_IsDead(bool bOldIsDead)
{
	// 客户端收到服务器复制的死亡状态后应用对应视觉（模拟代理上看不到死亡 GA，由此兜底）
	if (bIsDead)
	{
		ApplyDeathState();
	}
	else
	{
		ApplyReviveState();
	}
}

void AOpenWorldARPGCharacter::ApplyDeathState()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void AOpenWorldARPGCharacter::ApplyReviveState()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->UpdateComponentVelocity();
	}
}

void AOpenWorldARPGCharacter::CorrectPawnOrient()
{
	FRotator CurrentRot = GetActorRotation();
	SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
}
