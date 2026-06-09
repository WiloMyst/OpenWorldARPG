// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/ARPGCharacterInterface.h"
#include "GameplayEffectTypes.h"
#include "Data/CharacterSaveData.h"
#include "PlayerCharacter.generated.h"

class UCharacterDataAsset;
class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UCharacterWeaponComponent;
class UAS_Player;
class UOpenWorldARPGCharacterMovementComponent;
class UBackpackComponent;
class UGameplayAbility;
class UClimbingComponent;
class AWeaponBase;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMovementInput, float, InputX, float, InputY);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthUpdated);

/** 退场完成委托：GA_SwapOut 结束时广播，由 Controller 监听以驱动 Possess */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwapOutCompleted, APlayerCharacter*, SwappedOutCharacter, FTransform, SwapTransform);

/**
 * 玩家角色。持有 ASC、AttributeSet，动态数据通过 FCharacterSaveData 唯一存储。
 */
UCLASS()
class OPENWORLDARPG_API APlayerCharacter : public AOpenWorldARPGCharacter, public IAbilitySystemInterface, public IARPGCharacterInterface
{
	GENERATED_BODY()

public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaTime) override;

	/** 客户端收到 PlayerState 同步后，重新初始化 ASC 的 AbilityActorInfo */
	virtual void OnRep_PlayerState() override;

	/** 注册网络同步属性 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 初始化 ---

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Initialization")
	void InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset);

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|State")
	void SetStandbyMode(bool bNewStandbyState);

	// --- 输入处理 ---

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInput(float InputX, float InputY);

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInputCompleted();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleInteractInput();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleSpacebarInput();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleJumpStartInput();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleJumpStopInput();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void ToggleGlide();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void ToggleAim();

	// --- 角色切换 ---

	/** [GA流水线] 旧接口，保留向后兼容，新逻辑应使用 GA_SwapOut */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void PerformSwapOut(FTransform& OutTransform);

	/** [GA流水线] 旧接口，保留向后兼容，新逻辑应使用 GA_SwapIn */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void PerformSwapIn(const FTransform& InTransform);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnSwapFX(FVector Location);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetStandbyMode(bool bNewStandbyState);

	void ApplyStandbyMode(bool bNewStandbyState);

	UFUNCTION()
	void OnRep_CharacterIdentityTags();

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Swap")
	bool CanSwapOut() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Swap")
	bool CanSwapIn() const;

	// --- 角色切换 (GA 流水线) ---

	/** 退场完成委托：GA_SwapOut::EndAbility 时调用 NotifySwapOutCompleted 触发 */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCharacter|Swap")
	FOnSwapOutCompleted OnSwapOutCompleted;

	/** 由 GA_SwapOut 调用，广播退场完成委托（仅服务器端调用） */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void NotifySwapOutCompleted(const FTransform& SwapTransform);

	/** 设置待传入的出场 Transform，Controller 在激活 GA_SwapIn 前调用 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void SetPendingSwapInTransform(const FTransform& InTransform);

	/** 获取待传入的出场 Transform，GA_SwapIn 在 ActivateAbility 中读取 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Swap")
	const FTransform& GetPendingSwapInTransform() const { return PendingSwapInTransform; }

	/** GA_SwapOut 的能力类，Controller 通过此配置激活退场技能 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TSubclassOf<UGameplayAbility> SwapOutAbilityClass;

	/** GA_SwapIn 的能力类，Controller 通过此配置激活出场技能 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TSubclassOf<UGameplayAbility> SwapInAbilityClass;

	// --- 动画与物理 ---

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupBaseBehaviorAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupAimAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupPhysicsAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void ClearPhysicsAnimLayers();

	// 用于通知客户端重置摄像机延迟和物理表现
	UFUNCTION(Client, Reliable)
	void Client_ResetCameraAndPhysics(FRotator TargetRotation);

	// --- 越界处理 ---

	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	// --- Getters ---

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Weapon")
	USceneComponent* GetWeaponRestSocket() const { return WeaponRestSocket; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	UAS_Player* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Movement")
	UOpenWorldARPGCharacterMovementComponent* GetCustomMovementComp() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	UCharacterDataAsset* GetDataSourceAsset() const { return DataSourceAsset; }

	virtual UCharacterDataAsset* GetCharacterDataAsset_Implementation() const override { return DataSourceAsset; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetCharacterTag() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetStandbyStateTag() const { return StandbyStateTag; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TSubclassOf<AWeaponBase> GetWeaponBlueprint() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetNormalAttackMontages() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetHeavyAttackMontages() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetPlungeAttackMontages() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	int32 GetCharacterLevel() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	int32 GetConstellationLevel() const;

	FCharacterSaveData& GetRuntimeDataRef() { return RuntimeData; }
	const FCharacterSaveData& GetRuntimeData() const { return RuntimeData; }

	const TArray<TSubclassOf<UGameplayAbility>>& GetPermanentAbilitiesToActivate() const { return PermanentAbilitiesToActivate; }

protected:
	void NormalMovement(float InputX, float InputY);
	void ResetGlideCooldown();
	void OnMeshLoaded(const UCharacterDataAsset* DataAsset);
	virtual void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
	void AdjustAimingCamera(float DeltaTime);

	UFUNCTION()
	void OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|State")
	virtual void HandleDeath_Implementation() override;

public:
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
	TObjectPtr<UCharacterWeaponComponent> WeaponComponent;

	// --- 输入缓存 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputX = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputY = 0.0f;

	// --- GAS ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAS_Player> AttributeSet;

	// --- 数据 ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterDataAsset> DataSourceAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "PlayerCharacter|Data")
	FCharacterSaveData RuntimeData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CharacterIdentityTags, Category = "PlayerCharacter|Data")
	FGameplayTagContainer CharacterIdentityTags;

	// --- 运行时缓存 ---

	TArray<TSubclassOf<UGameplayAbility>> PermanentAbilitiesToActivate;

	TArray<TWeakObjectPtr<UActorComponent>> TickingComponentsSnapshot;

	/** 待传入的出场 Transform，由 Controller 在激活 GA_SwapIn 前写入 */
	FTransform PendingSwapInTransform;

	// --- 配置：Tags ---

	/** 待机状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag StandbyStateTag;

	/** 死亡能力标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag DeathAbilityTag;

	/** 不可控制状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag UncontrollableStateTag;

	/** 爬行状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag ClimbingStateTag;

	/** 滑翔状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlidingStateTag;

	/** 跳跃开始事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStartEventTag;

	/** 跳跃结束事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStopEventTag;

	/** 滑翔开始事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStartEventTag;

	/** 滑翔结束事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStopEventTag;

	/** 瞄准开始事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStartEventTag;

	/** 瞄准结束事件标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStopEventTag;

	/** 瞄准状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimingStateTag;

	/** 跳跃后多久允许开伞 */
    UPROPERTY(EditDefaultsOnly, Category = "PlayerCharacter|Movement|Glide")
    float GlideCooldownAfterJump = 0.3f;

	// --- 配置：角色切换 ---

	/** 角色切换特效系统 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TObjectPtr<UNiagaraSystem> CharacterSwapFX;

	/** 角色切换特效位置偏移 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

	/** 角色切换特效缩放 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

	/** 防止切换标签标签容器 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FGameplayTagContainer PreventSwitchTags;

	/** 允许切换出的移动模式数组 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TArray<TEnumAsByte<EMovementMode>> AllowedSwapOutMovementModes;

	// --- 配置：摄像机 ---

	/** 摄像机插值速度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float CameraInterpSpeed = 10.0f;

	/** 正常目标臂长度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float NormalTargetArmLength = 400.0f;

	/** 瞄准目标臂长度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float AimingTargetArmLength = 150.0f;

	/** 正常目标臂偏移 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector NormalSocketOffset = FVector::ZeroVector;

	/** 瞄准目标臂偏移 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector AimingSocketOffset = FVector(0.0f, 50.0f, 20.0f);

	// --- 摄像机运行时状态 (事件驱动) ---

	float CurrentTargetArmLength = 400.0f;
	FVector CurrentTargetSocketOffset = FVector::ZeroVector;

	FTimerHandle JumpGlideCooldownTimer;
    bool bCanGlideAfterJump = true;
};
