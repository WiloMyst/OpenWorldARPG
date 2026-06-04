// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Data/CharacterDataAsset.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/CharacterWeaponComponent.h"
#include "Weapons/WeaponBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "Managers/GameAssetManagerSubsystem.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOpenWorldARPGCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 恢复被删掉的 Tick 启用，否则平滑摄像机和跌落检测彻底失效！
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 恢复被删掉的核心移动设置，解决鼠标无法转向和角色不转身的问题！
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
	// 恢复被删掉的武器摇臂参数
	WeaponSpringArm->bDoCollisionTest = false;
	WeaponSpringArm->bEnableCameraLag = true;
	WeaponSpringArm->CameraLagSpeed = 10.0f;

	WeaponRestSocket = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRestSocket"));
	WeaponRestSocket->SetupAttachment(WeaponSpringArm);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAS_Player>(TEXT("AttributeSet"));
}

void APlayerCharacter::InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset)
{
	if (!InDataAsset) return;

	RuntimeData = InSaveData;
	DataSourceAsset = InDataAsset;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (AttributeSet)
		{
			AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
		}

		// [被恢复的逻辑]: 添加角色类型 Tag（解决 A-Pose 的核心原因）
		if (InDataAsset->ElementType.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->ElementType, 1);
		}
		if (InDataAsset->WeaponType.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->WeaponType, 1);
		}

		// [被恢复的逻辑]: 赋予通用的基础技能（跑、跳等，也是脱离A-Pose的关键）
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
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		TSoftObjectPtr<USkeletalMesh> MeshToLoad = InDataAsset->CharacterMesh;
		if (MeshToLoad.IsPending())
		{
			FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
			UCharacterDataAsset* CapturedDataAsset = InDataAsset;
			StreamableManager.RequestAsyncLoad(MeshToLoad.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([this, CapturedDataAsset]()
					{
						OnMeshLoaded(CapturedDataAsset);
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

		SetActorHiddenInGame(true);

		if (GetMesh() && GetMesh()->GetAnimInstance())
		{
			GetMesh()->GetAnimInstance()->StopAllMontages(0.1f);
		}
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

	CheckKillZAndRespawn();
	AdjustAimingCamera(DeltaTime);
}

void APlayerCharacter::HandleMovementInput(float InputX, float InputY)
{
	CurrentInputX = InputX;
	CurrentInputY = InputY;

	if (!Controller || !GetCharacterMovement()) return;

	OnPlayerMovementInput.Broadcast(InputX, InputY);

	uint8 Mode = GetCharacterMovement()->MovementMode;
	if (Mode == MOVE_Walking || Mode == MOVE_NavWalking || Mode == MOVE_Falling)
	{
		NormalMovement(InputX, InputY);
	}
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

void APlayerCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	// 恢复丢失的激活死亡能力的逻辑
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

void APlayerCharacter::CheckKillZAndRespawn()
{
	// 恢复整个庞大的跌落保护、清空速度与返回出生点的流水线
	if (GetActorLocation().Z <= KillZThreshold)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}

		if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
		{
			if (AActor* PlayerStart = GameMode->FindPlayerStart(GetController(), RespawnPlayerStartTag.ToString()))
			{
				FTransform StartTransform = PlayerStart->GetActorTransform();
				SetActorLocationAndRotation(
					StartTransform.GetLocation(),
					StartTransform.GetRotation(),
					false, nullptr, ETeleportType::TeleportPhysics
				);

				if (AController* PC = GetController())
				{
					PC->SetControlRotation(StartTransform.Rotator());
				}

				if (AbilitySystemComponent && RespawnCancelAbilityTags.IsValid())
				{
					AbilitySystemComponent->CancelAbilities(&RespawnCancelAbilityTags);
				}
			}
		}
	}
}

void APlayerCharacter::AdjustAimingCamera(float DeltaTime)
{
	if (!CameraBoom) return;

	bool bIsAiming = false;
	if (AbilitySystemComponent && AimingStateTag.IsValid())
	{
		bIsAiming = AbilitySystemComponent->HasMatchingGameplayTag(AimingStateTag);
	}

	const float TargetArmLength = bIsAiming ? AimingTargetArmLength : NormalTargetArmLength;
	const FVector TargetSocketOffset = bIsAiming ? AimingSocketOffset : NormalSocketOffset;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaTime, CameraInterpSpeed);
}

void APlayerCharacter::HandleDeath()
{
	// 恢复武器的隐形逻辑
	if (UCharacterWeaponComponent* WeaponComp = FindComponentByClass<UCharacterWeaponComponent>())
	{
		WeaponComp->SetWeaponHidden(true);
	}
}