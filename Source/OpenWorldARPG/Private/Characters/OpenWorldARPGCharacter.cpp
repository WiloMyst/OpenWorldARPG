// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/OpenWorldARPGCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AOpenWorldARPGCharacter::AOpenWorldARPGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)


	TeamId = FGenericTeamId(10);

}

FGenericTeamId AOpenWorldARPGCharacter::GetGenericTeamId() const
{
	return TeamId;
}

void AOpenWorldARPGCharacter::HandleDeath_Implementation()
{
	// 防止重入：死亡处理只执行一次
	if (bIsDead) return;
	bIsDead = true;

	// 底层物理碰撞剥离：关闭胶囊体碰撞并忽略所有通道，避免死亡后形成隐形墙
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	// 停止移动组件：先立即停止速度，再禁用移动模式
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void AOpenWorldARPGCharacter::HandleRevive_Implementation()
{
	// 复活：重置死亡标记
	bIsDead = false;

	// 恢复胶囊体碰撞为正常 Pawn 碰撞
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// 恢复移动组件：使用 Falling 模式让角色自然落地
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

