// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/WeaponManagerComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/TargetingComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"

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

	WeaponManagerComponent = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManagerComponent"));

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));

	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAS_Player>(TEXT("AttributeSet"));
}

void APlayerCharacter::InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterVisualDataAsset* InVisualData, UCharacterCombatDataAsset* InCombatData, const FCharacterRegistryRow& InRegistryRow)
{
	if (!InVisualData || !InCombatData) return;

	if (UOpenWorldARPGCharacterMovementComponent* CustomMC = GetCustomMovementComp())
	{
		CustomMC->CacheOwnerReferences();
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

		// ==========================================
		// 身份 Tag 注入（从 FCharacterRegistryRow 直接获取）
		// ==========================================
		// 新架构优势：InitializeCharacter 直接接收 RegistryRow，
		// 不再需要反查 DataTable 获取 ElementType/WeaponType。
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

		// 状态驱动同步：将元素/武器 Tag 写入 Replicated 属性
		CharacterIdentityTags.Reset();
		if (ElementTypeTag.IsValid())
		{
			CharacterIdentityTags.AddTag(ElementTypeTag);
		}
		if (WeaponTypeTag.IsValid())
		{
			CharacterIdentityTags.AddTag(WeaponTypeTag);
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>())
			{
				if (UCharacterGeneralDataAsset* GeneralData = AssetManager->GetPlayerCharacterGeneralAbilityDataAsset())
				{
					UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Granting GeneralAbilities: %d"),
						GeneralData->GeneralAbilityClasses.Num());

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

		// ==========================================
		// 战斗属性 GE 注入（从 UCharacterCombatDataAsset 读取）
		// ==========================================
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

		// ==========================================
		// 天赋技能赋予（字典化遍历 + 智能被动激活）
		// ==========================================
		// 遍历 CharacterTalents 字典中的所有天赋配置，
		// 通过解析 Key 的 GameplayTag 层级自动区分主动/被动天赋：
		// - 如果 Tag 是 Ability.Passive 的子标签（如 Ability.Passive.CritUp），
		//   则判定为被动天赋，赋予后自动激活
		// - 其他 Tag（如 Ability.Attack.Normal）为主动天赋，由玩家输入触发
		//
		// 使用 MatchesTag 替代字符串 Contains 检查：
		// - MatchesTag 是 GameplayTag 系统的原生 API，语义精确
		// - Ability.Passive.CritUp.MatchesTag(Ability.Passive) == true
		// - Ability.Attack.Normal.MatchesTag(Ability.Passive) == false
		const FGameplayTag PassiveTag = FGameplayTag::RequestGameplayTag(FName("Ability.Type.Passive"));

		for (const auto& TalentPair : InCombatData->CharacterTalents)
		{
			const FGameplayTag& TalentTag = TalentPair.Key;
			const FTalentConfig& Talent = TalentPair.Value;

			UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] GrantTalent: Tag=%s, AbilityClasses=%d"),
				TalentTag.IsValid() ? *TalentTag.ToString() : TEXT("INVALID"),
				Talent.AbilityClasses.Num());

			// 智能被动判定：Tag 是 Ability.Passive 的子标签即为被动天赋
			bool bIsPassive = TalentTag.IsValid() && TalentTag.MatchesTag(PassiveTag);

			for (const TSubclassOf<UGameplayAbility>& AbilityClass : Talent.AbilityClasses)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
					AbilitySpec.DynamicAbilityTags.AddTag(TalentTag);
					AbilitySystemComponent->GiveAbility(AbilitySpec);

					// 被动天赋加入自动激活列表
					if (bIsPassive)
					{
						PermanentAbilitiesToActivate.Add(AbilityClass);
					}

					UE_LOG(LogTemp, Warning, TEXT("[PlayerChar]   Granted TalentAbility: %s, TalentTag=%s, Passive=%s"),
						*AbilityClass->GetName(), *TalentTag.ToString(), bIsPassive ? TEXT("true") : TEXT("false"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[PlayerChar]   TalentAbility class is NULL for Tag=%s!"), *TalentTag.ToString());
				}
			}
		}

		// 赋予角色切换 GA（SwapOut / SwapIn）
		if (SwapOutAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(SwapOutAbilityClass, 1, INDEX_NONE, this));
		}
		if (SwapInAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(SwapInAbilityClass, 1, INDEX_NONE, this));
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

		// 注册游泳 Tag 变化回调 (事件驱动，供动画蓝图读取)
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
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Calling InitializeCharacterWeapon, VisualDataAsset=%s"),
			VisualDataAsset ? *VisualDataAsset->GetName() : TEXT("NULL"));
		WeaponManagerComponent->InitializeCharacterWeapon();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerChar] WeaponManagerComponent is NULL at InitCharacterData!"));
	}
}

void APlayerCharacter::OnMeshLoaded(const UCharacterVisualDataAsset* VisualData)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent || !VisualData) return;

	USkeletalMesh* LoadedMesh = VisualData->CharacterMesh.Get();
	if (!LoadedMesh) return;

	MeshComponent->SetSkeletalMesh(LoadedMesh);

	// 解析 TSoftClassPtr 获取动画蓝图类
	if (VisualData->AnimationBlueprint.IsValid())
	{
		MeshComponent->SetAnimInstanceClass(VisualData->AnimationBlueprint.Get());
	}
	else if (!VisualData->AnimationBlueprint.IsNull())
	{
		// TSoftClassPtr 尚未加载，同步加载（仅在初始化时，可接受）
		UClass* AnimBPClass = VisualData->AnimationBlueprint.LoadSynchronous();
		if (AnimBPClass)
		{
			MeshComponent->SetAnimInstanceClass(AnimBPClass);
		}
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

	if (UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = GetCustomMovementComp())
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
	if (UInteractionComponent* Backpack = FindComponentByClass<UInteractionComponent>())
	{
		Backpack->PickUpItem();
	}
}

void APlayerCharacter::HandleSpacebarInput()
{
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = GetCustomMovementComp();
    if (!CustomMoveComp) return;

    // 1. 如果在攀爬，执行跳跃（退出攀爬）
    if (CustomMoveComp->IsClimbing())
    {
        HandleJumpStartInput();
        return;
    }

    // 2. 如果在滑翔中按空格，关闭滑翔
    if (CustomMoveComp->IsGliding())
    {
        ToggleGlide();
        return;
    }

    // 3. 可能在空中（Falling），且没有处于跳跃初期的保护冷却中，就可以开伞
    if (CustomMoveComp->IsFalling())
    {
        if (bCanGlideAfterJump)
        {
            ToggleGlide();
        }
        // 如果 bCanGlideAfterJump 为 false (刚按完跳跃)，则吃掉这个输入，无事发生
        return;
    }

    // 4. 其他常规情况（如地面），执行跳跃
    HandleJumpStartInput();
}

void APlayerCharacter::HandleJumpStartInput()
{
	// 攀爬状态下按跳跃：发送停止攀爬事件，由 GA_ClimbBase 监听并清理现场后退出
	if (AbilitySystemComponent && ClimbingStateTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(ClimbingStateTag))
	{
		if (StopClimbEventTag.IsValid())
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, StopClimbEventTag, FGameplayEventData());
		}
		return;
	}

	// 发送跳跃 GAS 事件（由 GA_JumpBase 监听并激活）
    if (JumpStartEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, JumpStartEventTag, FGameplayEventData());

        // 主动跳跃后，短暂禁止开伞
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
	// 发送停止跳跃 GAS 事件（由 GA_JumpBase 监听并结束跳跃）
	if (JumpStopEventTag.IsValid())
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, JumpStopEventTag, FGameplayEventData());
	}
}

void APlayerCharacter::ToggleGlide()
{
	if (!AbilitySystemComponent) return;

	// 滑翔只能在空中下落时启动，地面按空格不应触发滑翔
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);

		// 如果当前不在滑翔状态，且不在空中（下落），则不允许启动滑翔
		if (!bIsGliding && !MoveComp->IsFalling())
		{
			return;
		}
	}

	// 状态翻转：当前滑翔则停止，否则启动
	bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);
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

void APlayerCharacter::SetupBaseBehaviorAnimLayers()
{
	if (!VisualDataAsset || !GetMesh()) return;

	UClass* AnimLayerClass = VisualDataAsset->BaseBehaviorAnimLayers.IsValid()
		? VisualDataAsset->BaseBehaviorAnimLayers.Get()
		: VisualDataAsset->BaseBehaviorAnimLayers.LoadSynchronous();

	if (AnimLayerClass && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void APlayerCharacter::SetupAimAnimLayers()
{
	if (!VisualDataAsset || !GetMesh()) return;

	UClass* AnimLayerClass = VisualDataAsset->AimAnimLayers.IsValid()
		? VisualDataAsset->AimAnimLayers.Get()
		: VisualDataAsset->AimAnimLayers.LoadSynchronous();

	if (AnimLayerClass && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void APlayerCharacter::SetupPhysicsAnimLayers()
{
	if (!VisualDataAsset || !GetMesh()) return;

	UClass* AnimLayerClass = VisualDataAsset->PhysicsAnimLayers.IsValid()
		? VisualDataAsset->PhysicsAnimLayers.Get()
		: VisualDataAsset->PhysicsAnimLayers.LoadSynchronous();

	if (AnimLayerClass && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void APlayerCharacter::ClearPhysicsAnimLayers()
{
	if (!VisualDataAsset || !GetMesh()) return;

	UClass* AnimLayerClass = VisualDataAsset->BaseBehaviorAnimLayers.IsValid()
		? VisualDataAsset->BaseBehaviorAnimLayers.Get()
		: VisualDataAsset->BaseBehaviorAnimLayers.LoadSynchronous();

	if (AnimLayerClass && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void APlayerCharacter::Client_ResetCameraAndPhysics_Implementation(FRotator TargetRotation)
{
	CurrentTargetArmLength = NormalTargetArmLength;
	CurrentTargetSocketOffset = NormalSocketOffset;

	if (Controller)
	{
		Controller->SetControlRotation(TargetRotation);
	}
}

void APlayerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	// 1. 传递逻辑必须只在服务器执行
	if (!HasAuthority()) return;

	// 2. 立即清除速度，防止传送后带有惯性
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	// 3. 寻找出生点
	AActor* StartSpot = nullptr;
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		StartSpot = GM->FindPlayerStart(GetController());
	}

	if (StartSpot)
	{
		FRotator SpawnRotation = StartSpot->GetActorRotation();

		// 4. 物理和位置传递（带上出生点的朝向）
		SetActorLocationAndRotation(
			StartSpot->GetActorLocation(),
			SpawnRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		// 5. 【核心修改】在服务器重置控制器的 ControlRotation
		// 这样服务器上的 AI 视线、射线检测以及下一次网络同步的基准朝向都会变正确
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetControlRotation(SpawnRotation);
		}

		// 6. 通知客户端执行视觉防拉影和本地视角的瞬间切断
		Client_ResetCameraAndPhysics(SpawnRotation);
	}
	else
	{
		Super::FellOutOfWorld(dmgType);
	}
}

// --- Getters ---

FGameplayTag APlayerCharacter::GetCharacterTag() const { return RuntimeData.CharacterTag; }

TSubclassOf<AWeaponBase> APlayerCharacter::GetWeaponBlueprint() const
{
	if (!VisualDataAsset) return nullptr;
	// 解析 TSoftClassPtr：如果已加载直接返回，否则同步加载
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

const FTalentConfig* APlayerCharacter::FindTalentConfig(const FGameplayTag& TalentTag) const
{
	if (!CombatDataAsset) return nullptr;
	return CombatDataAsset->CharacterTalents.Find(TalentTag);
}

int32 APlayerCharacter::GetCharacterLevel() const { return RuntimeData.CharacterLevel; }
int32 APlayerCharacter::GetConstellationLevel() const { return RuntimeData.ConstellationLevel; }
UOpenWorldARPGCharacterMovementComponent* APlayerCharacter::GetCustomMovementComp() const { return Cast<UOpenWorldARPGCharacterMovementComponent>(GetCharacterMovement()); }

void APlayerCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
	{
		OnHealthUpdated.Broadcast();
		HandleDeath_Implementation();
	}
}

void APlayerCharacter::AdjustAimingCamera(float DeltaTime)
{
	if (!AbilitySystemComponent) return;

	float TargetArmLength = NormalTargetArmLength;
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
	// 事件驱动：Tag 变化时自动切换武器显示状态
	if (Tag == AimingStateTag)
	{
		if (NewCount > 0)
		{
			if (WeaponManagerComponent) WeaponManagerComponent->WeaponToHand();
		}
	}
}

void APlayerCharacter::HandleDeath_Implementation()
{
	if (DeathAbilityTag.IsValid() && AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(DeathAbilityTag);
	}

	SetActorTickEnabled(false);

	if (USkeletalMeshComponent* SKMesh = GetMesh())
	{
		if (UAnimInstance* AnimInst = SKMesh->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.1f);
		}
	}
}

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
