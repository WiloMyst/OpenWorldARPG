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

	// UI 扩展组件：作为 Gameplay 与 UI 之间的桥梁
	HeroUIExtensionComp = CreateDefaultSubobject<UHeroUIExtensionComponent>(TEXT("HeroUIExtensionComp"));

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
		// 绑定 CMC 翻越蒙太奇请求委托：CMC 只管物理位移，动画播放由 Character 从 VisualDataAsset 获取
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

		// ==========================================
		// 其他 GA 能力赋予（仅需 GA 类，无需连招图）
		// ==========================================
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : InCombatData->GenericAbilities)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
				UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] Granted GenericAbility: %s"), *AbilityClass->GetName());
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

	SetupUpperBodyLayers();
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APlayerCharacter::SyncAttributesToSaveData()
{
	// 【数据同步通道】：GAS AttributeSet -> RuntimeData 快照 -> CharacterManagerSubsystem
	// UI 面板展示的唯一数据源是 RuntimeData 中的属性快照，而非实时 GAS 查询。
	// 调用时机：角色属性发生永久性变化（升级、换武器、突破、装备圣遗物等）时。

	if (!AttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("SyncAttributesToSaveData: AttributeSet 无效，跳过同步"));
		return;
	}

	UAS_Player* PlayerAS = Cast<UAS_Player>(AttributeSet);
	if (!PlayerAS) return;

	// 1. 将 GAS 算好的真实属性值反写到 RuntimeData 快照
	RuntimeData.MaxHealth = PlayerAS->GetMaxHealth();

	// TODO: 当前 AS_Player 尚未定义 Attack/Defense/CritRate/CritDamage 属性，
	// 待 GAS 扩展后在下方补充对应反写逻辑：
	// RuntimeData.Attack      = PlayerAS->GetAttack();
	// RuntimeData.Defense     = PlayerAS->GetDefense();
	// RuntimeData.CritRate    = PlayerAS->GetCritRate();
	// RuntimeData.CritDamage  = PlayerAS->GetCritDamage();

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
	if (InteractionComponent)
	{
		InteractionComponent->Interact();
	}
}

void APlayerCharacter::HandleSpacebarInput()
{
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = GetCustomMovementComp();
    if (!CustomMoveComp) return;

    // 1. 如果在攀爬，发送专属攀爬跳跃事件（由 GA_ClimbJump 处理向上冲刺/脱墙后空翻）
    if (CustomMoveComp->IsClimbing())
    {
        if (AbilitySystemComponent && ClimbJumpEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, ClimbJumpEventTag, FGameplayEventData());
        }
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
	// 发送跳跃 GAS 事件（由 GA_JumpBase 监听并激活）
	// 注意：攀爬状态下的跳跃已在 HandleSpacebarInput() 中被拦截并发送 ClimbJumpEventTag，
	//       由 GA_ClimbJumpBase 处理，不会走到这里。
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

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	bool bIsGliding = GlidingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(GlidingStateTag);

	// 如果当前不在滑翔状态，需要满足条件才能启动滑翔
	if (!bIsGliding)
	{
		// 条件1：必须在空中（下落状态）
		if (!MoveComp->IsFalling())
		{
			return;
		}

		// 条件2：离地面高度必须大于最小开伞高度，太低不允许开伞
		if (UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = GetCustomMovementComp())
		{
			float DistanceToGround = CustomMoveComp->GetDistanceToGround();
			if (DistanceToGround >= 0.0f && DistanceToGround < CustomMoveComp->MinGlideStartHeight)
			{
				return;
			}
		}
	}

	// 状态翻转：当前滑翔则停止，否则启动
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
	// 从 Controller 读取玩家期望的镜头距离（切换角色后保持不变）
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
	// 1. 传递逻辑必须只在服务器执行
	if (!HasAuthority()) return;

	// 2. 结束所有已激活的 GA（攀爬、钩索等），清理状态 Tag 和 GE
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 3. 武器回到背上
	if (WeaponManagerComponent)
	{
		WeaponManagerComponent->WeaponToBack();
	}

	// 4. 立即清除速度，防止传送后带有惯性
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Falling);
	}

	// 5. 寻找出生点
	AActor* StartSpot = nullptr;
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		StartSpot = GM->FindPlayerStart(GetController());
	}

	if (StartSpot)
	{
		FRotator SpawnRotation = StartSpot->GetActorRotation();

		// 6. 物理和位置传递（带上出生点的朝向）
		SetActorLocationAndRotation(
			StartSpot->GetActorLocation(),
			SpawnRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		// 7. 【核心修改】在服务器重置控制器的 ControlRotation
		// 这样服务器上的 AI 视线、射线检测以及下一次网络同步的基准朝向都会变正确
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetControlRotation(SpawnRotation);
		}

		// 8. 通知客户端执行视觉防拉影和本地视角的瞬间切断
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

		// 事件驱动：优先通过 GAS 事件触发死亡能力(GA_DieBase)，由能力负责表现层(蒙太奇/布娃娃)
		if (AbilitySystemComponent && DieEventTag.IsValid())
		{
			FGameplayEventData EventData;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, DieEventTag, EventData);
		}
		else
		{
			// 兜底：没有死亡能力时直接执行底层清理
			HandleDeath_Implementation();
		}
	}
}

void APlayerCharacter::AdjustAimingCamera(float DeltaTime)
{
	if (!AbilitySystemComponent) return;

	// 正常状态下目标为 Controller 上玩家滚轮调节的期望距离（切换角色后保持不变）；瞄准状态下强制覆盖为瞄准距离
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
	// 调用基类：设置 bIsDead=true、禁用碰撞(NoCollision+IgnoreAll)、停止移动(StopMovementImmediately+DisableMovement)
	Super::HandleDeath_Implementation();

	// 禁用玩家输入，防止死后继续移动
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	// 注意：不再调用 SetActorTickEnabled(false) 和 StopAllMontages()，
	// 表现层（死亡蒙太奇/布娃娃）由 GA_DieBase 全权负责。
	// 注意：死亡状态 Tag (Character.State.Dead) 由 GA_DieBase 的 ActivationOwnedTags 自动管理，
	// 不在此处手动 AddLooseGameplayTag，避免与 GA 生命周期冲突。
}

void APlayerCharacter::HandleRevive_Implementation()
{
	Super::HandleRevive_Implementation();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}
}

UWeaponManagerComponent* APlayerCharacter::GetWeaponManagerComponent_Implementation() const
{
	return WeaponManagerComponent;
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

// ============================================================================
// 攀爬蒙太奇播放
// ============================================================================

void APlayerCharacter::OnClimbUpMontageRequested(UAnimMontage* MontageToPlay)
{
	// CMC 广播 nullptr 时，从 VisualDataAsset 获取 ClimbUpMontage
	UAnimMontage* ClimbMontage = MontageToPlay;
	if (!ClimbMontage && VisualDataAsset)
	{
		if (!VisualDataAsset->ClimbUpMontage.IsNull())
		{
			ClimbMontage = VisualDataAsset->ClimbUpMontage.LoadSynchronous();
		}
	}

	if (!ClimbMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerChar] OnClimbUpMontageRequested: ClimbUpMontage is null in VisualDataAsset"));
		return;
	}

	// 播放翻越蒙太奇
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			// 绑定蒙太奇结束回调：蒙太奇播放完毕后调用 FinishClimbUp 恢复移动模式
			AnimInst->OnMontageEnded.AddDynamic(this, &APlayerCharacter::OnClimbUpMontageEnded);
			AnimInst->Montage_Play(ClimbMontage, 1.0f);
		}
	}
}

void APlayerCharacter::OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!Montage) return;

	// 只处理 ClimbUp 蒙太奇
	if (VisualDataAsset && Montage == VisualDataAsset->ClimbUpMontage.Get())
	{
		// 解绑回调，避免影响其他蒙太奇
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				AnimInst->OnMontageEnded.RemoveDynamic(this, &APlayerCharacter::OnClimbUpMontageEnded);
			}
		}

		// 调用 CMC::FinishClimbUp 恢复移动模式
		if (UOpenWorldARPGCharacterMovementComponent* CustomMC = GetCustomMovementComp())
		{
			CustomMC->FinishClimbUp();
		}

		// 发送停止攀爬事件，结束 GA_ClimbBase（清理攀爬 Tag 和体力 GE）
		if (ClimbStopEventTag.IsValid() && AbilitySystemComponent)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, ClimbStopEventTag, FGameplayEventData());
		}
	}
}

// ============================================================================
// 载具驾驶：上车准备
// ============================================================================
void APlayerCharacter::PrepareForDriving(AActor* VehicleActor, FName SocketName)
{
	if (!VehicleActor) return;

	bIsDriving = true;

	// 关闭移动组件
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		CMC->SetMovementMode(MOVE_None);
	}

	// 修改 Capsule 碰撞：关闭碰撞，防止干扰 Chaos 载具物理
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 安全获取载具的 Mesh
	if (AWheeledVehiclePawnBase* VehiclePawn = Cast<AWheeledVehiclePawnBase>(VehicleActor))
	{
		AttachToComponent(
			VehiclePawn->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			SocketName);
	}
}

// ============================================================================
// 载具驾驶：下车恢复
// ============================================================================
void APlayerCharacter::EndDriving(FVector ExitLocation)
{
	bIsDriving = false;

	// Detach
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 移动到下车位置
	SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 恢复移动组件
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_Walking);
	}

	// 恢复 Capsule 默认碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}
