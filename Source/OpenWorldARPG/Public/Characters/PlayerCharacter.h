// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/ARPGCharacterInterface.h"
#include "GameplayEffectTypes.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterSaveData.h"
#include "MotionWarpingComponent.h"
#include "PlayerCharacter.generated.h"

class AWheeledVehiclePawnBase;
class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UWeaponManagerComponent;
class UAS_Player;
class UOpenWorldARPGCharacterMovementComponent;
class UInteractionComponent;
class UTargetingComponent;
class UHeroUIExtensionComponent;
class UGameplayAbility;
class AWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMovementInput, float, InputX, float, InputY);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwapOutCompleted, APlayerCharacter*, SwappedOutCharacter, FTransform, SwapTransform);

/**
 * 玩家角色。持有 ASC、AttributeSet，动态数据通过 FCharacterSaveData 唯一存储。
 *
 * 数据架构（UI / 表现 / 战斗 三层解耦）：
 * - VisualDataAsset：外观/动画数据，美术负责
 * - CombatDataAsset：战斗/天赋数据，战斗策划负责
 * - FCharacterRegistryRow (DataTable)：UI 元数据 + 身份 Tag，策划负责
 * 三层独立签出，Perforce 不锁死。运行时由 GameAssetManagerSubsystem 异步加载后传入 InitializeCharacter。
 */
UCLASS()
class OPENWORLDARPG_API APlayerCharacter : public AOpenWorldARPGCharacter, public IAbilitySystemInterface, public IARPGCharacterInterface
{
	GENERATED_BODY()

public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaTime) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	// --- 初始化 ---

	void InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterVisualDataAsset* InVisualData, UCharacterCombatDataAsset* InCombatData, const FCharacterRegistryRow& InRegistryRow);
	void SetStandbyMode(bool bNewStandbyState);

	// --- 输入处理 ---

	void HandleMovementInput(float InputX, float InputY);
	void HandleMovementInputCompleted();
	void HandleInteractInput();
	void HandleSpacebarInput();
	void HandleJumpStartInput();
	void HandleJumpStopInput();
	void ToggleGlide();
	void ToggleAim();

	// --- 网络同步 ---

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetStandbyMode(bool bNewStandbyState);

	UFUNCTION(Client, Reliable)
	void Client_ResetCameraAndPhysics(FRotator TargetRotation);

	void ApplyStandbyMode(bool bNewStandbyState);

	UFUNCTION()
	void OnRep_CharacterIdentityTags();

	// --- 角色切换 (GA 流水线) ---

	void NotifySwapOutCompleted(const FTransform& SwapTransform);
	FGameplayTag GetSwapOutEventTag() const { return SwapOutEventTag; }
	FGameplayTag GetSwapInEventTag() const { return SwapInEventTag; }

	// --- 动画 ---

	void SetupUpperBodyLayers();

	// --- 载具驾驶 ---

	void PrepareForDriving(AActor* VehicleActor, FName SocketName);
	void EndDriving(FVector ExitLocation);
	bool IsDriving() const { return bIsDriving; }

	// --- 接口实现 (IAbilitySystemInterface / IARPGCharacterInterface) ---

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	virtual UWeaponManagerComponent* GetWeaponManagerComponent_Implementation() const override { return WeaponManagerComponent; }
	virtual UTargetingComponent* GetTargetingComponent_Implementation() const override { return TargetingComponent; }
	virtual UOpenWorldARPGCharacterMovementComponent* GetCustomMovementComponent_Implementation() const override;
	virtual UHeroUIExtensionComponent* GetHeroUIExtensionComponent_Implementation() const override { return HeroUIExtensionComp; }
	virtual UCharacterVisualDataAsset* GetVisualDataAsset_Implementation() const override { return VisualDataAsset; }
	virtual UCharacterCombatDataAsset* GetCombatDataAsset_Implementation() const override { return CombatDataAsset; }

	// --- 数据查询 ---

	// 组件
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	USceneComponent* GetWeaponRestSocket() const { return WeaponRestSocket; }
	UAS_Player* GetAttributeSet() const { return AttributeSet; }
	UMotionWarpingComponent* GetMotionWarpingComp() const { return MotionWarpingComp; }

	// 输入状态
	float GetCurrentInputX() const { return CurrentInputX; }
	float GetCurrentInputY() const { return CurrentInputY; }

	// 角色身份与战斗数据
	FGameplayTag GetCharacterTag() const { return RuntimeData.CharacterTag; }
	FGameplayTag GetStandbyStateTag() const { return StandbyStateTag; }
	TSubclassOf<AWeaponBase> GetWeaponBlueprint() const;
	const FTalentConfig* GetTalentConfig(const FGameplayTag& TalentTag) const { return CombatDataAsset ? CombatDataAsset->CharacterTalents.Find(TalentTag) : nullptr; }
	int32 GetCharacterLevel() const { return RuntimeData.CharacterLevel; }
	int32 GetConstellationLevel() const { return RuntimeData.ConstellationLevel; }

	// 运行时数据
	FCharacterSaveData& GetRuntimeDataRef() { return RuntimeData; }
	const FCharacterSaveData& GetRuntimeData() const { return RuntimeData; }
	const TArray<TSubclassOf<UGameplayAbility>>& GetPermanentAbilitiesToActivate() const { return PermanentAbilitiesToActivate; }
	void SyncAttributesToSaveData();

protected:
	// --- 内部逻辑 ---

	void NormalMovement(float InputX, float InputY);
	void ResetGlideCooldown();
	void OnMeshLoaded(const UCharacterVisualDataAsset* VisualData);
	void AdjustAimingCamera(float DeltaTime);

	// --- GAS 回调 ---

	virtual void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void OnSwimmingTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void OnClimbUpMontageRequested(UAnimMontage* MontageToPlay);

	UFUNCTION()
	void OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// --- 死亡 / 复活 ---

	virtual void HandleDeath_Implementation() override;
	virtual void HandleRevive_Implementation() override;

public:
	// --- 事件委托 ---

	UPROPERTY(BlueprintAssignable, Category = "PlayerCharacter|Swap")
	FOnSwapOutCompleted OnSwapOutCompleted;

	UPROPERTY(BlueprintAssignable, Category = "PlayerCharacter|Events")
	FOnPlayerMovementInput OnPlayerMovementInput;

	UPROPERTY(BlueprintAssignable, Category = "PlayerCharacter|Events")
	FOnHealthUpdated OnHealthUpdated;

protected:
	// --- 组件 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Weapon")
	TObjectPtr<USpringArmComponent> WeaponSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Weapon")
	TObjectPtr<USceneComponent> WeaponRestSocket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Weapon")
	TObjectPtr<UWeaponManagerComponent> WeaponManagerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Interaction")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Targeting")
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|MotionWarping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|UI")
	TObjectPtr<UHeroUIExtensionComponent> HeroUIExtensionComp;

	// --- GAS ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAS_Player> AttributeSet;

	// --- 数据 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterVisualDataAsset> VisualDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterCombatDataAsset> CombatDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "PlayerCharacter|Data")
	FCharacterSaveData RuntimeData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CharacterIdentityTags, Category = "PlayerCharacter|Data")
	FGameplayTagContainer CharacterIdentityTags;

	// --- 输入缓存 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputX = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputY = 0.0f;

	// --- 运行时缓存 ---

	TArray<TSubclassOf<UGameplayAbility>> PermanentAbilitiesToActivate;
	TArray<TWeakObjectPtr<UActorComponent>> TickingComponentsSnapshot;

	float CurrentTargetArmLength = 400.0f;
	FVector CurrentTargetSocketOffset = FVector::ZeroVector;
	FTimerHandle JumpGlideCooldownTimer;
	bool bCanGlideAfterJump = true;

	// --- 运行时状态 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|State")
	bool bIsSwimming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|State")
	bool bIsFastSwimming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|State")
	bool bIsDriving = false;

	// --- 配置：状态 Tag ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag StandbyStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag UncontrollableStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag ClimbingStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlidingStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimingStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag SwimmingStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag FastSwimmingStateTag;

	// --- 配置：事件 Tag ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag DieEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag ClimbStopEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag ClimbJumpEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStopEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStopEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStopEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag SwapOutEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag SwapInEventTag;

	// --- 配置：摄像机 ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float CameraInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float NormalTargetArmLength = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float AimingTargetArmLength = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector NormalSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector AimingSocketOffset = FVector(0.0f, 50.0f, 20.0f);

	// --- 配置：移动 ---

	UPROPERTY(EditDefaultsOnly, Category = "PlayerCharacter|Movement|Glide")
	float GlideCooldownAfterJump = 0.3f;
};
