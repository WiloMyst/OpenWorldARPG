// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Data/CharacterDataAsset.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/MovementStateMachineComponent.h"
#include "Components/CharacterWeaponComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "Managers/GameAssetManagerSubsystem.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOpenWorldARPGCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	WeaponSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("WeaponSpringArm"));
	WeaponSpringArm->SetupAttachment(RootComponent);
	WeaponSpringArm->bDoCollisionTest = false;
	WeaponSpringArm->bEnableCameraLag = true;
	WeaponSpringArm->CameraLagSpeed = 10.0f;

	WeaponRestSocket = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRestSocket"));
	WeaponRestSocket->SetupAttachment(WeaponSpringArm);

	MovementStateMachine = CreateDefaultSubobject<UMovementStateMachineComponent>(TEXT("MovementStateMachine"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAS_Player>(TEXT("AttributeSet"));
}

void APlayerCharacter::InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset)
{
	if (!InDataAsset) return;

	if (UOpenWorldARPGCharacterMovementComponent* CustomMC = GetCustomMovementComp())
	{
		CustomMC->CacheOwnerReferences();
	}

	RuntimeData = InSaveData;
	DataSourceAsset = InDataAsset;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (AttributeSet)
		{
			AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
		}

		if (InDataAsset->ElementType.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->ElementType, 1);
		}
		if (InDataAsset->WeaponType.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->WeaponType, 1);
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>())
			{
				if (UCharacterGeneralDataAsset* GeneralData = AssetManager->GetPlayerCharacterGeneralAbilityDataAsset())
				{
					for (TSubclassOf<UGameplayAbility> AbilityClass : GeneralData->GeneralPermanentAbilityClasses)
					{
						if (AbilityClass)
						{
							AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
							PermanentAbilitiesToActivate.Add(AbilityClass);
						}
					}
					for (TSubclassOf<UGameplayAbility> AbilityClass : GeneralData->GeneralAbilityClasses)
					{
						if (AbilityClass) AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
					}
				}
			}
		}

		if (InDataAsset->BaseAttributesEffect)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InDataAsset->BaseAttributesEffect, 1.0f, EffectContext);
			if (SpecHandle.IsValid()) AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		for (const auto& Pair : InDataAsset->AscensionBonusEffects)
		{
			if (Pair.Key <= RuntimeData.AscensionLevel && Pair.Value)
			{
				FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
				EffectContext.AddSourceObject(this);
				FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(Pair.Value, 1.0f, EffectContext);
				if (SpecHandle.IsValid()) AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}

		auto GrantTalentAbilities = [&](const FTalentConfig& Talent)
			{
				for (const TSubclassOf<UGameplayAbility>& AbilityClass : Talent.AbilityClasses)
				{
					if (AbilityClass)
					{
						FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
						AbilitySpec.DynamicAbilityTags.AddTag(Talent.TalentTag);
						AbilitySystemComponent->GiveAbility(AbilitySpec);
					}
				}
			};

		GrantTalentAbilities(InDataAsset->NormalAttack);
		GrantTalentAbilities(InDataAsset->HeavyAttack);
		GrantTalentAbilities(InDataAsset->PlungeAttack);
		GrantTalentAbilities(InDataAsset->SkillAttack);
		GrantTalentAbilities(InDataAsset->UltimateAttack);

		for (const FTalentConfig& PassiveTalent : InDataAsset->PassiveTalents)
		{
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : PassiveTalent.AbilityClasses)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
					AbilitySpec.DynamicAbilityTags.AddTag(PassiveTalent.TalentTag);
					AbilitySystemComponent->GiveAbility(AbilitySpec);
					PermanentAbilitiesToActivate.Add(AbilityClass);
				}
			}
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : PermanentAbilitiesToActivate)
		{
			if (AbilityClass)
			{
				for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
				{
					if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass && !Spec.IsActive())
					{
						AbilitySystemComponent->TryActivateAbility(Spec.Handle);
					}
				}
			}
		}

		// 注册瞄准 Tag 变化回调 (事件驱动，替代 Tick 中每帧查询)
		if (AimingStateTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(AimingStateTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &APlayerCharacter::OnAimingTagChanged);
		}
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		TSoftObjectPtr<USkeletalMesh> MeshToLoad = InDataAsset->CharacterMesh;
		if (MeshToLoad.IsPending())
		{
			FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
			UCharacterDataAsset* CapturedDataAsset = InDataAsset;
			TWeakObjectPtr<APlayerCharacter> WeakThis = this;
			StreamableManager.RequestAsyncLoad(MeshToLoad.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, CapturedDataAsset]() {
					if (WeakThis.IsValid()) {
						WeakThis->OnMeshLoaded(CapturedDataAsset);
					}
				}));
		}
		else if (MeshToLoad.IsValid())
		{
			OnMeshLoaded(InDataAsset);
		}
	}

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAS_Player::GetHealthAttribute()).AddUObject(this, &APlayerCharacter::OnHealthAttributeChanged);
	}

	if (UCharacterWeaponComponent* WeaponComp = FindComponentByClass<UCharacterWeaponComponent>())
	{
		WeaponComp->InitializeCharacterWeapon();
	}
}

void APlayerCharacter::OnMeshLoaded(const UCharacterDataAsset* DataAsset)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent || !DataAsset) return;

	USkeletalMesh* LoadedMesh = DataAsset->CharacterMesh.Get();
	if (!LoadedMesh) return;

	MeshComponent->SetSkeletalMesh(LoadedMesh);

	// 必须在 DataAsset 中配置 AnimationBlueprint，不再回退到 PlayerCharacterAnimInstance
	if (DataAsset->AnimationBlueprint)
	{
		MeshComponent->SetAnimInstanceClass(DataAsset->AnimationBlueprint);
	}

	SetupBaseBehaviorAnimLayers();
	SetupAimAnimLayers();
	SetupPhysicsAnimLayers();
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APlayerCharacter::SetStandbyMode(bool bNewStandbyState)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	if (bNewStandbyState)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();

		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
		if (USkeletalMeshComponent* SKMesh = GetMesh())
		{
			SKMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SKMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}

		TickingComponentsSnapshot.Empty();
		TArray<UActorComponent*> AllComponents;
		GetComponents(AllComponents);
		for (UActorComponent* Comp : AllComponents)
		{
			if (Comp->PrimaryComponentTick.bCanEverTick && Comp != MoveComp && Comp->IsActive() && Comp->IsComponentTickEnabled())
			{
				TickingComponentsSnapshot.Add(Comp);
				Comp->SetComponentTickEnabled(false);
			}
		}
		SetActorTickEnabled(false);
		MoveComp->SetComponentTickEnabled(false);

		if (UCharacterWeaponComponent* WeaponComp = FindComponentByClass<UCharacterWeaponComponent>())
		{
			WeaponComp->SetWeaponHidden(true);
		}

		if (USkeletalMeshComponent* SKMesh = GetMesh())
		{
			if (UAnimInstance* AnimInst = SKMesh->GetAnimInstance())
			{
				AnimInst->StopAllMontages(0.1f);
			}
			SKMesh->bNoSkeletonUpdate = true;
			SKMesh->SetUpdateAnimationInEditor(false);
		}

		SetActorHiddenInGame(true);
	}
	else
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}
		if (USkeletalMeshComponent* SKMesh = GetMesh())
		{
			SKMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SKMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}

		SetActorHiddenInGame(false);

		if (USkeletalMeshComponent* SKMesh = GetMesh())
		{
			SKMesh->bNoSkeletonUpdate = false;
			SKMesh->SetUpdateAnimationInEditor(true);
		}

		if (UCharacterWeaponComponent* WeaponComp = FindComponentByClass<UCharacterWeaponComponent>())
		{
			WeaponComp->SetWeaponHidden(false);
		}

		MoveComp->SetMovementMode(MOVE_Walking);

		for (const TWeakObjectPtr<UActorComponent>& WeakComp : TickingComponentsSnapshot)
		{
			if (UActorComponent* Comp = WeakComp.Get())
			{
				Comp->SetComponentTickEnabled(true);
			}
		}
		TickingComponentsSnapshot.Empty();
		SetActorTickEnabled(true);
		MoveComp->SetComponentTickEnabled(true);
	}
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 仅做摄像机插值，不查 GAS Tag (事件驱动)
	AdjustAimingCamera(DeltaTime);
}

void APlayerCharacter::HandleMovementInput(float InputX, float InputY)
{
	CurrentInputX = InputX;
	CurrentInputY = InputY;

	if (!Controller || !GetCharacterMovement()) return;

	// 攀爬状态下：InputY 映射为墙面上下移动（Z轴），InputX 映射为墙面左右移动
	// CMC 在 PhysClimbing 中通过 ConsumeInputVector 读取，投影到墙面平面
	// 不广播 OnPlayerMovementInput，避免 ClimbingComponent 在攀爬中触发多余的检测
	if (MovementStateMachine && MovementStateMachine->IsClimbing())
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 左右：沿相机朝向的水平右方向
		AddMovementInput(RightDirection, InputX);
		// 上下：直接用 Z 轴，投影到墙面后即为墙面垂直方向
		AddMovementInput(FVector::UpVector, InputY);
		return;
	}

	// 始终走引擎原生输入管线 (AddMovementInput)
	NormalMovement(InputX, InputY);

	// 广播输入事件供 ClimbingComponent 等使用
	OnPlayerMovementInput.Broadcast(InputX, InputY);
}

void APlayerCharacter::HandleMovementInputCompleted()
{
	CurrentInputX = 0.0f;
	CurrentInputY = 0.0f;
	OnPlayerMovementInput.Broadcast(0.0f, 0.0f);
}

void APlayerCharacter::NormalMovement(float InputX, float InputY)
{
	if (!Controller) return;

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, InputY);
	AddMovementInput(RightDirection, InputX);
}

void APlayerCharacter::SetupBaseBehaviorAnimLayers()
{
	if (DataSourceAsset && DataSourceAsset->BaseBehaviorAnimLayers)
	{
		if (USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			CharacterMesh->LinkAnimClassLayers(DataSourceAsset->BaseBehaviorAnimLayers);
		}
	}
}

void APlayerCharacter::SetupAimAnimLayers()
{
	if (DataSourceAsset && DataSourceAsset->AimAnimLayers)
	{
		if (USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			CharacterMesh->LinkAnimClassLayers(DataSourceAsset->AimAnimLayers);
		}
	}
}

void APlayerCharacter::SetupPhysicsAnimLayers()
{
	if (DataSourceAsset && DataSourceAsset->PhysicsAnimLayers)
	{
		if (USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			CharacterMesh->LinkAnimClassLayers(DataSourceAsset->PhysicsAnimLayers);
		}
	}
}

void APlayerCharacter::ClearPhysicsAnimLayers()
{
	if (DataSourceAsset && DataSourceAsset->PhysicsAnimLayers)
	{
		if (USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			CharacterMesh->UnlinkAnimClassLayers(DataSourceAsset->PhysicsAnimLayers);
		}
	}
}

FGameplayTag APlayerCharacter::GetCharacterTag() const { return RuntimeData.CharacterTag; }
TSubclassOf<AWeaponBase> APlayerCharacter::GetWeaponBlueprint() const { return DataSourceAsset ? DataSourceAsset->WeaponBlueprint : nullptr; }
TArray<TSoftObjectPtr<UAnimMontage>> APlayerCharacter::GetNormalAttackMontages() const { return DataSourceAsset ? DataSourceAsset->NormalAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }
TArray<TSoftObjectPtr<UAnimMontage>> APlayerCharacter::GetHeavyAttackMontages() const { return DataSourceAsset ? DataSourceAsset->HeavyAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }
TArray<TSoftObjectPtr<UAnimMontage>> APlayerCharacter::GetPlungeAttackMontages() const { return DataSourceAsset ? DataSourceAsset->PlungeAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }
int32 APlayerCharacter::GetCharacterLevel() const { return RuntimeData.CharacterLevel; }
int32 APlayerCharacter::GetConstellationLevel() const { return RuntimeData.ConstellationLevel; }
UOpenWorldARPGCharacterMovementComponent* APlayerCharacter::GetCustomMovementComp() const { return Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()); }

UMovementStateMachineComponent* APlayerCharacter::GetMovementStateMachine() const { return MovementStateMachine; }

void APlayerCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
	{
		if (AbilitySystemComponent && DeathAbilityTag.IsValid())
		{
			FGameplayTagContainer TagContainer(DeathAbilityTag);
			AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer, true);
		}
		HandleDeath();
	}
	OnHealthUpdated.Broadcast();
}

// ==========================================
// 越界处理 (引擎原生回调，替代 Tick 中的 CheckKillZAndRespawn)
// ==========================================

void APlayerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	// 引擎在角色跌出 KillZ 边界时自动调用此函数
	// 不需要每帧检测 Z 坐标，不需要客户端获取 GameMode
	// 只需通知死亡，重生逻辑由 GameMode / PlayerController 负责
	HandleDeath();
}

// ==========================================
// 瞄准摄像机 (事件驱动，Tick 只做插值)
// ==========================================

void APlayerCharacter::OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Tag 被添加时 NewCount > 0，被移除时 NewCount == 0
	if (NewCount > 0)
	{
		CurrentTargetArmLength = AimingTargetArmLength;
		CurrentTargetSocketOffset = AimingSocketOffset;
	}
	else
	{
		CurrentTargetArmLength = NormalTargetArmLength;
		CurrentTargetSocketOffset = NormalSocketOffset;
	}
}

void APlayerCharacter::AdjustAimingCamera(float DeltaTime)
{
	if (!CameraBoom) return;

	// 纯插值，不查 GAS Tag
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, CurrentTargetArmLength, DeltaTime, CameraInterpSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, CurrentTargetSocketOffset, DeltaTime, CameraInterpSpeed);
}

void APlayerCharacter::HandleDeath()
{
	if (UCharacterWeaponComponent* WeaponComp = FindComponentByClass<UCharacterWeaponComponent>())
	{
		WeaponComp->SetWeaponHidden(true);
	}
}
