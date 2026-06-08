// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Data/CharacterDataAsset.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/MovementStateMachineComponent.h"
#include "Components/CharacterWeaponComponent.h"
#include "Components/BackpackComponent.h"
#include "Components/ClimbingComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Net/UnrealNetwork.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOpenWorldARPGCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 网络同步基础配置：角色实体需要在客户端和服务端之间同步
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

	WeaponComponent = CreateDefaultSubobject<UCharacterWeaponComponent>(TEXT("WeaponComponent"));

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

		// 通过 Multicast 将元素/武器 Tag 同步到所有客户端
		// AddLooseGameplayTag 本身不同步，需要显式通知客户端添加
		Multicast_AddLooseGameplayTags(InDataAsset->ElementType, InDataAsset->WeaponType);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>())
			{
				if (UCharacterGeneralDataAsset* GeneralData = AssetManager->GetPlayerCharacterGeneralAbilityDataAsset())
				{
					UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Granting GeneralAbilities: Permanent=%d, Normal=%d"),
						GeneralData->GeneralPermanentAbilityClasses.Num(), GeneralData->GeneralAbilityClasses.Num());

					for (TSubclassOf<UGameplayAbility> AbilityClass : GeneralData->GeneralPermanentAbilityClasses)
					{
						if (AbilityClass)
						{
							FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
							PermanentAbilitiesToActivate.Add(AbilityClass);
							UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Granted PermanentAbility: %s"),
								*AbilityClass->GetName());
						}
					}
					for (TSubclassOf<UGameplayAbility> AbilityClass : GeneralData->GeneralAbilityClasses)
					{
						if (AbilityClass)
						{
							AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
							UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Granted GeneralAbility: %s"),
								*AbilityClass->GetName());
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[PlayerChar] CharacterGeneralDataAsset is NULL! Cannot grant general abilities."));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[PlayerChar] GameAssetManagerSubsystem is NULL!"));
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
				UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] GrantTalent: Tag=%s, AbilityClasses=%d, Montages=%d"),
					Talent.TalentTag.IsValid() ? *Talent.TalentTag.ToString() : TEXT("INVALID"),
					Talent.AbilityClasses.Num(), Talent.Montages.Num());

				for (const TSubclassOf<UGameplayAbility>& AbilityClass : Talent.AbilityClasses)
				{
					if (AbilityClass)
					{
						FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
						AbilitySpec.DynamicAbilityTags.AddTag(Talent.TalentTag);
						FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
						UE_LOG(LogTemp, Warning, TEXT("[PlayerChar]   Granted TalentAbility: %s, TalentTag=%s"),
							*AbilityClass->GetName(), *Talent.TalentTag.ToString());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[PlayerChar]   TalentAbility class is NULL for Tag=%s!"), *Talent.TalentTag.ToString());
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

	if (WeaponComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Calling InitializeCharacterWeapon, DataSourceAsset=%s"),
			DataSourceAsset ? *DataSourceAsset->GetName() : TEXT("NULL"));
		WeaponComponent->InitializeCharacterWeapon();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerChar] WeaponComponent is NULL at InitCharacterData!"));
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
	// 服务器端调用时，通过 Multicast 同步到所有客户端
	if (HasAuthority())
	{
		Multicast_SetStandbyMode(bNewStandbyState);
	}
	else
	{
		// 客户端直接执行（由 Multicast 回调触发）
		ApplyStandbyMode(bNewStandbyState);
	}
}

void APlayerCharacter::Multicast_SetStandbyMode_Implementation(bool bNewStandbyState)
{
	ApplyStandbyMode(bNewStandbyState);
}

void APlayerCharacter::Multicast_AddLooseGameplayTags_Implementation(FGameplayTag ElementTypeTag, FGameplayTag WeaponTypeTag)
{
	if (AbilitySystemComponent)
	{
		if (ElementTypeTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(ElementTypeTag, 1);
		}
		if (WeaponTypeTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(WeaponTypeTag, 1);
		}
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

		// 仅在服务器端保存和关闭 Tick（客户端不需要管理 TickingComponentsSnapshot）
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

		if (WeaponComponent)
		{
			WeaponComponent->SetWeaponHidden(true);
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

		if (WeaponComponent)
		{
			WeaponComponent->SetWeaponHidden(false);
		}

		MoveComp->SetMovementMode(MOVE_Walking);

		// 仅在服务器端恢复 Tick
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

	// 仅做摄像机插值，不查 GAS Tag (事件驱动)
	AdjustAimingCamera(DeltaTime);
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 客户端收到 PlayerState 后，需要重新初始化 ASC 的 AbilityActorInfo
	// 否则客户端的 ASC 无法正确预测技能和接收 GameplayEffect
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlayerCharacter, RuntimeData);
}

void APlayerCharacter::HandleMovementInput(float InputX, float InputY)
{
	CurrentInputX = InputX;
	CurrentInputY = InputY;

	if (!Controller || !GetCharacterMovement()) return;

	// 不可控状态拦截 (原 Controller 越权逻辑，现回归 Character)
	if (UncontrollableStateTag.IsValid() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(UncontrollableStateTag))
	{
		return;
	}

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

void APlayerCharacter::HandleInteractInput()
{
	// 拾取逻辑由 Character 内部查找 BackpackComponent 执行
	// Controller 不再 FindComponentByClass 微操
	if (UBackpackComponent* Backpack = FindComponentByClass<UBackpackComponent>())
	{
		Backpack->PickUpItem();
	}
}

void APlayerCharacter::HandleJumpStartInput()
{
	UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] HandleJumpStartInput called"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] MovementMode=%d, IsFalling=%d, IsWalking=%d, IsMovingOnGround=%d"),
			(int32)MoveComp->MovementMode.GetValue(), MoveComp->IsFalling(), MoveComp->IsWalking(), MoveComp->IsMovingOnGround());
	}

	// 诊断：检查 ASC 当前拥有的所有 Tag
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AllTags;
		AbilitySystemComponent->GetOwnedGameplayTags(AllTags);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ASC Tags: %s"), *AllTags.ToStringSimple());
	}

	// 攀爬状态下按跳跃：退出攀爬 (原 Controller 越权逻辑，现回归 Character)
	if (AbilitySystemComponent && ClimbingStateTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(ClimbingStateTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Climbing state detected, exiting climb"));
		if (UClimbingComponent* ClimbComp = FindComponentByClass<UClimbingComponent>())
		{
			ClimbComp->ExitClimb();
		}
	}

	// 发送跳跃 GAS 事件
	if (JumpStartEventTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Sending JumpStartEvent: %s"), *JumpStartEventTag.ToString());

		// 诊断：检查 GA_Jump 是否可以激活
		if (AbilitySystemComponent)
		{
			for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
			{
				if (Spec.IsActive())
				{
					UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Active GA: %s"),
						*Spec.Ability->GetName());
				}
			}
		}

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, JumpStartEventTag, FGameplayEventData());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerChar] JumpStartEventTag is INVALID! Cannot send jump event."));
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

	UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ToggleGlide called"));

	// 滑翔只能在空中下落时启动，地面按空格不应触发滑翔
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ToggleGlide: bIsGliding=%d, IsFalling=%d, MovementMode=%d, GravityScale=%.2f, VelocityZ=%.2f"),
			bIsGliding, MoveComp->IsFalling(), (int32)MoveComp->MovementMode.GetValue(), MoveComp->GravityScale, MoveComp->Velocity.Z);

		// 如果当前不在滑翔状态，且不在空中（下落），则不允许启动滑翔
		if (!bIsGliding && !MoveComp->IsFalling())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ToggleGlide: BLOCKED - not falling and not already gliding"));
			return;
		}

		// 如果角色正在上升（刚跳跃），不允许启动滑翔
		// 防止跳跃和滑翔绑定同一按键时，跳跃后瞬间触发滑翔
		if (!bIsGliding && MoveComp->Velocity.Z > 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ToggleGlide: BLOCKED - character is moving upward (Velocity.Z=%.2f)"), MoveComp->Velocity.Z);
			return;
		}
	}

	// 状态翻转：当前滑翔则停止，否则启动
	bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);
	FGameplayTag TagToSend = bIsGliding ? GlideStopEventTag : GlideStartEventTag;

	UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] ToggleGlide: sending event %s (bIsGliding=%d)"), *TagToSend.ToString(), bIsGliding);

	if (TagToSend.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagToSend, FGameplayEventData());
	}
}

void APlayerCharacter::ToggleAim()
{
	if (!AbilitySystemComponent) return;

	// 状态翻转：当前瞄准则停止，否则启动
	bool bIsAiming = AimingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(AimingStateTag);
	FGameplayTag TagToSend = bIsAiming ? AimStopEventTag : AimStartEventTag;

	if (TagToSend.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, TagToSend, FGameplayEventData());
	}
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

void APlayerCharacter::HandleDeath_Implementation()
{
	Super::HandleDeath_Implementation();

	if (WeaponComponent)
	{
		WeaponComponent->SetWeaponHidden(true);
	}
}

// ==========================================
// 角色切换流水线 (邮局原则：视觉表现归 Character，Possess 归 Controller)
// ==========================================

void APlayerCharacter::PerformSwapOut(FTransform& OutTransform)
{
	// 1. 保存当前 Transform 供新角色使用
	OutTransform = GetActorTransform();

	// 2. 在旧位置生成切换特效 (通过 NetMulticast 同步到所有客户端)
	if (CharacterSwapFX)
	{
		FVector SpawnFXLocation = OutTransform.GetLocation() + SwapFXLocationOffset;
		Multicast_SpawnSwapFX(SpawnFXLocation);
	}

	// 3. 进入待机休眠状态
	SetStandbyMode(true);
}

void APlayerCharacter::Multicast_SpawnSwapFX_Implementation(FVector Location)
{
	if (CharacterSwapFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			CharacterSwapFX,
			Location,
			FRotator::ZeroRotator,
			SwapFXScale
		);
	}
}

void APlayerCharacter::PerformSwapIn(const FTransform& InTransform)
{
	// 1. 先解除待机 (恢复碰撞和 Tick)
	SetStandbyMode(false);

	// 2. 再设置 Transform (避免在 NoCollision 状态下设置位置导致穿透问题)
	SetActorTransform(InTransform);
}

bool APlayerCharacter::CanSwapOut() const
{
	// 运动模式拦截：只允许在指定运动模式下切换下场
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (!AllowedSwapOutMovementModes.IsEmpty()
			&& !AllowedSwapOutMovementModes.Contains(MoveComp->MovementMode))
		{
			return false;
		}
	}

	return true;
}

bool APlayerCharacter::CanSwapIn() const
{
	// GAS 状态标签拦截：拥有 PreventSwitchTags 时禁止切换上场
	if (AbilitySystemComponent && PreventSwitchTags.IsValid())
	{
		if (AbilitySystemComponent->HasAnyMatchingGameplayTags(PreventSwitchTags))
		{
			return false;
		}
	}

	return true;
}
