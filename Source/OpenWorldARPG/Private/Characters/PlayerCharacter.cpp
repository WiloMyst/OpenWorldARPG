// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/WeaponManagerComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/TargetingComponent.h"
#include "Components/HeroUIExtensionComponent.h"
#include "Vehicles/WheeledVehiclePawnBase.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOpenWorldARPGCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;

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

	WeaponManagerComponent = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManagerComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));
	HeroUIExtensionComp = CreateDefaultSubobject<UHeroUIExtensionComponent>(TEXT("HeroUIExtensionComp"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAS_Player>(TEXT("AttributeSet"));
}

void APlayerCharacter::InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterVisualDataAsset* InVisualData, UCharacterCombatDataAsset* InCombatData, const FCharacterRegistryRow& InRegistryRow)
{
	if (!InVisualData || !InCombatData) return;

	if (UOpenWorldARPGCharacterMovementComponent* CustomMC = Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()))
	{
		CustomMC->CacheOwnerReferences();
		CustomMC->OnClimbUpMontageRequested.AddDynamic(this, &APlayerCharacter::OnClimbUpMontageRequested);
	}

	RuntimeData = InSaveData;
	VisualDataAsset = InVisualData;
	CombatDataAsset = InCombatData;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (AttributeSet)
		{
			AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
		}

		// 身份 Tag 注入
		FGameplayTag ElementTypeTag = InRegistryRow.ElementType;
		FGameplayTag WeaponTypeTag = InRegistryRow.WeaponType;

		if (ElementTypeTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(ElementTypeTag, 1);
		}
		if (WeaponTypeTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(WeaponTypeTag, 1);
		}

		CharacterIdentityTags.Reset();
		if (ElementTypeTag.IsValid()) CharacterIdentityTags.AddTag(ElementTypeTag);
		if (WeaponTypeTag.IsValid()) CharacterIdentityTags.AddTag(WeaponTypeTag);

		// 通用能力赋予
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>())
			{
				if (UCharacterGeneralDataAsset* GeneralData = AssetManager->GetPlayerCharacterGeneralAbilityDataAsset())
				{
					for (TSubclassOf<UGameplayAbility> AbilityClass : GeneralData->GeneralAbilityClasses)
					{
						if (AbilityClass)
						{
							AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
						}
					}
				}
			}
		}

		// 基础属性 GE
		if (InCombatData->BaseAttributesEffect)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InCombatData->BaseAttributesEffect, 1.0f, EffectContext);
			if (SpecHandle.IsValid()) AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		// 突破增益 GE
		for (const auto& Pair : InCombatData->AscensionBonusEffects)
		{
			if (Pair.Key <= RuntimeData.AscensionLevel && Pair.Value)
			{
				FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
				EffectContext.AddSourceObject(this);
				FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(Pair.Value, 1.0f, EffectContext);
				if (SpecHandle.IsValid()) AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}

		// 天赋技能赋予（Ability.Type.Passive 子标签的自动激活）
		const FGameplayTag PassiveTag = FGameplayTag::RequestGameplayTag(FName("Ability.Type.Passive"));

		for (const auto& TalentPair : InCombatData->CharacterTalents)
		{
			const FGameplayTag& TalentTag = TalentPair.Key;
			const FTalentConfig& Talent = TalentPair.Value;

			bool bIsPassive = TalentTag.IsValid() && TalentTag.MatchesTag(PassiveTag);

			for (const TSubclassOf<UGameplayAbility>& AbilityClass : Talent.AbilityClasses)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
					AbilitySpec.DynamicAbilityTags.AddTag(TalentTag);
					AbilitySystemComponent->GiveAbility(AbilitySpec);

					if (bIsPassive)
					{
						PermanentAbilitiesToActivate.Add(AbilityClass);
					}
				}
			}
		}

		// 通用 GA 赋予
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : InCombatData->GenericAbilities)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}

		// 激活被动天赋
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

		// 注册 Tag 变化回调
		if (AimingStateTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(AimingStateTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &APlayerCharacter::OnAimingTagChanged);
		}
		if (SwimmingStateTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(SwimmingStateTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &APlayerCharacter::OnSwimmingTagChanged);
		}
		if (FastSwimmingStateTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(FastSwimmingStateTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &APlayerCharacter::OnSwimmingTagChanged);
		}
	}

	// 异步加载骨骼网格体
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		TSoftObjectPtr<USkeletalMesh> MeshToLoad = InVisualData->CharacterMesh;
		if (MeshToLoad.IsPending())
		{
			FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
			UCharacterVisualDataAsset* CapturedVisualData = InVisualData;
			TWeakObjectPtr<APlayerCharacter> WeakThis = this;
			StreamableManager.RequestAsyncLoad(MeshToLoad.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, CapturedVisualData]() {
					if (WeakThis.IsValid()) {
						WeakThis->OnMeshLoaded(CapturedVisualData);
					}
				}));
		}
		else if (MeshToLoad.IsValid())
		{
			OnMeshLoaded(InVisualData);
		}
	}

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAS_Player::GetHealthAttribute()).AddUObject(this, &APlayerCharacter::OnHealthAttributeChanged);
	}

	if (WeaponManagerComponent)
	{
		WeaponManagerComponent->InitializeCharacterWeapon();
	}
}

void APlayerCharacter::OnMeshLoaded(const UCharacterVisualDataAsset* VisualData)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent || !VisualData) return;

	USkeletalMesh* LoadedMesh = VisualData->CharacterMesh.Get();
	if (!LoadedMesh) return;

	MeshComponent->SetSkeletalMesh(LoadedMesh);

	if (VisualData->AnimationBlueprint.IsValid())
	{
		MeshComponent->SetAnimInstanceClass(VisualData->AnimationBlueprint.Get());
	}
	else if (!VisualData->AnimationBlueprint.IsNull())
	{
		UClass* AnimBPClass = VisualData->AnimationBlueprint.LoadSynchronous();
		if (AnimBPClass)
		{
			MeshComponent->SetAnimInstanceClass(AnimBPClass);
		}
	}

	SetupUpperBodyLayers();
}

void APlayerCharacter::SyncAttributesToSaveData()
{
	if (!AttributeSet) return;

	UAS_Player* PlayerAS = Cast<UAS_Player>(AttributeSet);
	if (!PlayerAS) return;

	RuntimeData.MaxHealth = PlayerAS->GetMaxHealth();
	// TODO: 待 AS_Player 扩展后补充 Attack/Defense/CritRate/CritDamage 反写

	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UCharacterManagerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>())
			{
				Subsystem->SetCharacterSaveData(GetCharacterTag(), RuntimeData);
			}
		}
	}
}

void APlayerCharacter::SetStandbyMode(bool bNewStandbyState)
{
	if (HasAuthority())
	{
		Multicast_SetStandbyMode(bNewStandbyState);
	}
	else
	{
		ApplyStandbyMode(bNewStandbyState);
	}
}

void APlayerCharacter::Multicast_SetStandbyMode_Implementation(bool bNewStandbyState)
{
	ApplyStandbyMode(bNewStandbyState);
}

void APlayerCharacter::OnRep_CharacterIdentityTags()
{
	if (!AbilitySystemComponent) return;

	FGameplayTagContainer OldTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OldTags);

	for (const FGameplayTag& Tag : CharacterIdentityTags)
	{
		if (OldTags.HasTag(Tag))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
		}
		AbilitySystemComponent->AddLooseGameplayTag(Tag, 1);
	}
}

void APlayerCharacter::ApplyStandbyMode(bool bNewStandbyState)
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

		if (HasAuthority())
		{
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
		}
		if (WeaponManagerComponent)
		{
			WeaponManagerComponent->SetWeaponHidden(true);
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

		if (USkeletalMeshComponent* SKMesh = GetMesh())
		{
			SKMesh->bNoSkeletonUpdate = false;
			SKMesh->SetUpdateAnimationInEditor(true);
			SKMesh->TickAnimation(0.0f, false);
			SKMesh->RefreshBoneTransforms();
		}

		SetActorHiddenInGame(false);
		if (WeaponManagerComponent)
		{
			WeaponManagerComponent->SetWeaponHidden(false);
		}

		MoveComp->SetMovementMode(MOVE_Walking);

		if (HasAuthority())
		{
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
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdjustAimingCamera(DeltaTime);
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, RuntimeData);
	DOREPLIFETIME(APlayerCharacter, CharacterIdentityTags);
}

void APlayerCharacter::HandleMovementInput(float InputX, float InputY)
{
	CurrentInputX = InputX;
	CurrentInputY = InputY;

	if (!Controller || !GetCharacterMovement()) return;

	if (UncontrollableStateTag.IsValid() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(UncontrollableStateTag))
	{
		return;
	}

	if (UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()))
	{
		if (CustomMoveComp->IsClimbing())
		{
			AddMovementInput(GetActorRightVector(), InputX);
			AddMovementInput(FVector::UpVector, InputY);
			return;
		}
	}

	NormalMovement(InputX, InputY);
	OnPlayerMovementInput.Broadcast(InputX, InputY);
}

void APlayerCharacter::HandleMovementInputCompleted()
{
	CurrentInputX = 0.0f;
	CurrentInputY = 0.0f;
	OnPlayerMovementInput.Broadcast(0.0f, 0.0f);
}

void APlayerCharacter::HandleInteractInput()
{
	if (InteractionComponent)
	{
		InteractionComponent->Interact();
	}
}

void APlayerCharacter::HandleSpacebarInput()
{
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement());
    if (!CustomMoveComp) return;

    // 攀爬中：发送攀爬跳跃事件
    if (CustomMoveComp->IsClimbing())
    {
        if (AbilitySystemComponent && ClimbJumpEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, ClimbJumpEventTag, FGameplayEventData());
        }
        return;
    }

    // 滑翔中：关闭滑翔
    if (CustomMoveComp->IsGliding())
    {
        ToggleGlide();
        return;
    }

    // 下落中：开伞（需过了跳跃冷却）
    if (CustomMoveComp->IsFalling())
    {
        if (bCanGlideAfterJump)
        {
            ToggleGlide();
        }
        return;
    }

    // 地面：跳跃
    HandleJumpStartInput();
}

void APlayerCharacter::HandleJumpStartInput()
{
    if (JumpStartEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, JumpStartEventTag, FGameplayEventData());

        bCanGlideAfterJump = false;
        GetWorldTimerManager().SetTimer(
            JumpGlideCooldownTimer,
            this,
            &APlayerCharacter::ResetGlideCooldown,
            GlideCooldownAfterJump,
            false
        );
    }
}

void APlayerCharacter::HandleJumpStopInput()
{
	if (JumpStopEventTag.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, JumpStopEventTag, FGameplayEventData());
	}
}

void APlayerCharacter::ToggleGlide()
{
	if (!AbilitySystemComponent) return;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);

	if (!bIsGliding)
	{
		if (!MoveComp->IsFalling()) return;

		if (UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()))
		{
			float DistanceToGround = CustomMoveComp->GetDistanceToGround();
			if (DistanceToGround >= 0.0f && DistanceToGround < CustomMoveComp->MinGlideStartHeight)
			{
				return;
			}
		}
	}

	FGameplayTag TagToSend = bIsGliding ? GlideStopEventTag : GlideStartEventTag;
	if (TagToSend.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagToSend, FGameplayEventData());
	}
}

void APlayerCharacter::ToggleAim()
{
	if (!AbilitySystemComponent) return;

	if (AbilitySystemComponent->HasMatchingGameplayTag(AimingStateTag))
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(AimingStateTag);
	}
	else
	{
		AbilitySystemComponent->AddLooseGameplayTag(AimingStateTag);
	}
}

void APlayerCharacter::NotifySwapOutCompleted(const FTransform& SwapTransform)
{
	OnSwapOutCompleted.Broadcast(this, SwapTransform);
}

void APlayerCharacter::SetupUpperBodyLayers()
{
	if (!VisualDataAsset || !GetMesh()) return;

	UClass* AnimLayerClass = VisualDataAsset->UpperBodyLayers.IsValid()
		? VisualDataAsset->UpperBodyLayers.Get()
		: VisualDataAsset->UpperBodyLayers.LoadSynchronous();

	if (AnimLayerClass && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void APlayerCharacter::Client_ResetCameraAndPhysics_Implementation(FRotator TargetRotation)
{
	if (const AGameplayPlayerController* MainPC = Cast<AGameplayPlayerController>(GetController()))
	{
		CurrentTargetArmLength = MainPC->GetPlayerDesiredArmLength();
	}
	else
	{
		CurrentTargetArmLength = NormalTargetArmLength;
	}
	CurrentTargetSocketOffset = NormalSocketOffset;

	if (Controller)
	{
		Controller->SetControlRotation(TargetRotation);
	}
}

void APlayerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	if (!HasAuthority()) return;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (WeaponManagerComponent)
	{
		WeaponManagerComponent->WeaponToBack();
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	AActor* StartSpot = nullptr;
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		StartSpot = GM->FindPlayerStart(GetController());
	}

	if (StartSpot)
	{
		FRotator SpawnRotation = StartSpot->GetActorRotation();

		SetActorLocationAndRotation(
			StartSpot->GetActorLocation(),
			SpawnRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetControlRotation(SpawnRotation);
		}

		Client_ResetCameraAndPhysics(SpawnRotation);
	}
	else
	{
		Super::FellOutOfWorld(dmgType);
	}
}

// --- 非 inline Getter ---

TSubclassOf<AWeaponBase> APlayerCharacter::GetWeaponBlueprint() const
{
	if (!VisualDataAsset) return nullptr;
	if (VisualDataAsset->WeaponBlueprint.IsValid())
	{
		return VisualDataAsset->WeaponBlueprint.Get();
	}
	if (!VisualDataAsset->WeaponBlueprint.IsNull())
	{
		return VisualDataAsset->WeaponBlueprint.LoadSynchronous();
	}
	return nullptr;
}

UOpenWorldARPGCharacterMovementComponent* APlayerCharacter::GetCustomMovementComponent_Implementation() const
{
	return Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement());
}

// --- GAS 回调 ---

void APlayerCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
	{
		OnHealthUpdated.Broadcast();

		if (AbilitySystemComponent && DieEventTag.IsValid())
		{
			FGameplayEventData EventData;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, DieEventTag, EventData);
		}
		else
		{
			HandleDeath_Implementation();
		}
	}
}

void APlayerCharacter::AdjustAimingCamera(float DeltaTime)
{
	if (!AbilitySystemComponent) return;

	float TargetArmLength = NormalTargetArmLength;
	if (const AGameplayPlayerController* MainPC = Cast<AGameplayPlayerController>(GetController()))
	{
		TargetArmLength = MainPC->GetPlayerDesiredArmLength();
	}
	FVector TargetSocketOffset = NormalSocketOffset;

	if (AbilitySystemComponent->HasMatchingGameplayTag(AimingStateTag))
	{
		TargetArmLength = AimingTargetArmLength;
		TargetSocketOffset = AimingSocketOffset;
	}

	CurrentTargetArmLength = FMath::FInterpTo(CurrentTargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
	CurrentTargetSocketOffset = FMath::VInterpTo(CurrentTargetSocketOffset, TargetSocketOffset, DeltaTime, CameraInterpSpeed);

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentTargetArmLength;
		CameraBoom->SocketOffset = CurrentTargetSocketOffset;
	}
}

void APlayerCharacter::OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (Tag == AimingStateTag && NewCount > 0)
	{
		if (WeaponManagerComponent) WeaponManagerComponent->WeaponToHand();
	}
}

void APlayerCharacter::HandleDeath_Implementation()
{
	Super::HandleDeath_Implementation();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
}

void APlayerCharacter::HandleRevive_Implementation()
{
	Super::HandleRevive_Implementation();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}
}

// --- 内部逻辑 ---

void APlayerCharacter::NormalMovement(float InputX, float InputY)
{
	if (!Controller || !GetCharacterMovement()) return;

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, InputY);
	AddMovementInput(RightDirection, InputX);
}

void APlayerCharacter::ResetGlideCooldown()
{
	bCanGlideAfterJump = true;
}

void APlayerCharacter::OnSwimmingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (Tag == SwimmingStateTag)
	{
		bIsSwimming = NewCount > 0;
	}
	else if (Tag == FastSwimmingStateTag)
	{
		bIsFastSwimming = NewCount > 0;
	}
}

void APlayerCharacter::OnClimbUpMontageRequested(UAnimMontage* MontageToPlay)
{
	UAnimMontage* ClimbMontage = MontageToPlay;
	if (!ClimbMontage && VisualDataAsset)
	{
		if (!VisualDataAsset->ClimbUpMontage.IsNull())
		{
			ClimbMontage = VisualDataAsset->ClimbUpMontage.LoadSynchronous();
		}
	}

	if (!ClimbMontage) return;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			AnimInst->OnMontageEnded.AddDynamic(this, &APlayerCharacter::OnClimbUpMontageEnded);
			AnimInst->Montage_Play(ClimbMontage, 1.0f);
		}
	}
}

void APlayerCharacter::OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!Montage) return;

	if (VisualDataAsset && Montage == VisualDataAsset->ClimbUpMontage.Get())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				AnimInst->OnMontageEnded.RemoveDynamic(this, &APlayerCharacter::OnClimbUpMontageEnded);
			}
		}

		if (UOpenWorldARPGCharacterMovementComponent* CustomMC = Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()))
		{
			CustomMC->FinishClimbUp();
		}

		if (ClimbStopEventTag.IsValid() && AbilitySystemComponent)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, ClimbStopEventTag, FGameplayEventData());
		}
	}
}

// --- 载具驾驶 ---

void APlayerCharacter::PrepareForDriving(AActor* VehicleActor, FName SocketName)
{
	if (!VehicleActor) return;

	bIsDriving = true;

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		CMC->SetMovementMode(MOVE_None);
		CMC->GravityScale = 0.0f;
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 双重保险：关闭 SkeletalMesh 碰撞，彻底消除 Chaos Vehicle 物理干涉
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (AWheeledVehiclePawnBase* VehiclePawn = Cast<AWheeledVehiclePawnBase>(VehicleActor))
	{
		// 【核心修改】使用 KeepWorldTransform，角色保持在车门外，由 Motion Warping 处理后续位移
		AttachToComponent(
			VehiclePawn->GetMesh(),
			FAttachmentTransformRules::KeepWorldTransform,
			SocketName);
	}
}

void APlayerCharacter::EndDriving(FVector ExitLocation)
{
	bIsDriving = false;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_Walking);
		CMC->GravityScale = 1.0f;
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 恢复 SkeletalMesh 碰撞为 QueryOnly（角色 Mesh 不参与物理模拟，仅做查询）
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
