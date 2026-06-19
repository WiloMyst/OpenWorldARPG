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

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UWeaponManagerComponent;
class UAS_Player;
class UOpenWorldARPGCharacterMovementComponent;
class UInteractionComponent;
class UTargetingComponent;
class UGameplayAbility;
class AWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMovementInput, float, InputX, float, InputY);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthUpdated);

/** 退场完成委托：GA_SwapOut 结束时广播，由 Controller 监听以驱动 Possess */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwapOutCompleted, APlayerCharacter*, SwappedOutCharacter, FTransform, SwapTransform);

/**
 * 玩家角色。持有 ASC、AttributeSet，动态数据通过 FCharacterSaveData 唯一存储。
 *
 * 【数据架构：UI / 表现 / 战斗 三层解耦】
 * 角色持有两个已加载的数据资产引用：
 * - VisualDataAsset (UCharacterVisualDataAsset)：外观/动画数据，由美术负责
 * - CombatDataAsset (UCharacterCombatDataAsset)：战斗/天赋数据，由战斗策划负责
 * UI 展示数据（名称、头像、稀有度等）不存储在角色中，
 * 而是通过 FCharacterRegistryRow (DataTable) 按需查询。
 *
 * 这三层数据通过 FCharacterRegistryRow 的 TSoftObjectPtr 桥梁连接，
 * 运行时由 GameAssetManagerSubsystem 异步加载后传入 InitializeCharacter。
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

	/**
	 * 初始化角色。接收表现层和战斗层两个已加载的数据资产，以及注册表行引用。
	 *
	 * 【架构设计：三层解耦的初始化流】
	 * 旧架构：InitializeCharacter(SaveData, CharacterDataAsset)
	 *   - CharacterDataAsset 是超级资产，包含 UI/外观/战斗所有数据
	 *   - 美术和策划修改同一资产会互相锁死
	 *
	 * 新架构：InitializeCharacter(SaveData, VisualData, CombatData, RegistryRow)
	 *   - VisualData：纯外观数据（美术负责）
	 *   - CombatData：纯战斗数据（战斗策划负责）
	 *   - RegistryRow：UI 元数据 + 身份 Tag（策划A负责）
	 *   - 三层独立签出，Perforce 不锁死
	 *
	 * @param InSaveData 角色运行时存档数据
	 * @param InVisualData 已加载的外观表现数据资产
	 * @param InCombatData 已加载的战斗逻辑数据资产
	 * @param InRegistryRow 角色注册表行（UI 元数据 + 身份 Tag）
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Initialization")
	void InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterVisualDataAsset* InVisualData, UCharacterCombatDataAsset* InCombatData, const FCharacterRegistryRow& InRegistryRow);

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

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetStandbyMode(bool bNewStandbyState);

	void ApplyStandbyMode(bool bNewStandbyState);

	UFUNCTION()
	void OnRep_CharacterIdentityTags();

	// --- 角色切换 (GA 流水线) ---

	/** 退场完成委托：GA_SwapOutBase::EndAbility 时调用 NotifySwapOutCompleted 触发 */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCharacter|Swap")
	FOnSwapOutCompleted OnSwapOutCompleted;

	/** 由 GA_SwapOutBase 调用，广播退场完成委托（仅服务器端调用） */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void NotifySwapOutCompleted(const FTransform& SwapTransform);

	/** GA_SwapOut 的能力类，Controller 通过此配置激活退场技能 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TSubclassOf<UGameplayAbility> SwapOutAbilityClass;

	/** GA_SwapIn 的能力类，Controller 通过此配置激活出场技能 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TSubclassOf<UGameplayAbility> SwapInAbilityClass;

	// --- 动画与物理 ---

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupUpperBodyLayers();

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

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Targeting")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	UAS_Player* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Movement")
	UOpenWorldARPGCharacterMovementComponent* GetCustomMovementComp() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|MotionWarping")
	UMotionWarpingComponent* GetMotionWarpingComp() const { return MotionWarpingComp; }

	/** 获取外观表现数据资产（IARPGCharacterInterface 实现） */
	virtual UCharacterVisualDataAsset* GetVisualDataAsset_Implementation() const override { return VisualDataAsset; }

	/** 获取战斗逻辑数据资产（IARPGCharacterInterface 实现） */
	virtual UCharacterCombatDataAsset* GetCombatDataAsset_Implementation() const override { return CombatDataAsset; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetCharacterTag() const;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetStandbyStateTag() const { return StandbyStateTag; }

	/**
	 * 获取武器蓝图类（从 VisualDataAsset 解析 TSoftClassPtr）。
	 * 注意：TSoftClassPtr 需要在调用前已被加载，否则返回 nullptr。
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TSubclassOf<AWeaponBase> GetWeaponBlueprint() const;

	/**
	 * 通过天赋 Tag 查询天赋配置。
	 * 从 CombatDataAsset 的 CharacterTalents 字典中查找。
	 * @param TalentTag 天赋标签（如 Ability.Attack.Normal）
	 * @return 天赋配置指针，未找到返回 nullptr
	 */
	const FTalentConfig* FindTalentConfig(const FGameplayTag& TalentTag) const;

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
	void OnMeshLoaded(const UCharacterVisualDataAsset* VisualData);
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
	TObjectPtr<UWeaponManagerComponent> WeaponManagerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Interaction")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Targeting")
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|MotionWarping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

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

	// --- 数据：三层解耦 ---

	/** 外观表现数据资产（已加载的引用）。美术负责签出修改。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterVisualDataAsset> VisualDataAsset;

	/** 战斗逻辑数据资产（已加载的引用）。战斗策划负责签出修改。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterCombatDataAsset> CombatDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "PlayerCharacter|Data")
	FCharacterSaveData RuntimeData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CharacterIdentityTags, Category = "PlayerCharacter|Data")
	FGameplayTagContainer CharacterIdentityTags;

	// --- 运行时缓存 ---

	TArray<TSubclassOf<UGameplayAbility>> PermanentAbilitiesToActivate;

	TArray<TWeakObjectPtr<UActorComponent>> TickingComponentsSnapshot;

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

	/** 停止攀爬事件标签（攀爬中按跳跃时发送，与 CMC 检测到落地时发送的同一 Tag） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag StopClimbEventTag;

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

	// --- 游泳状态 (供动画蓝图读取，通过 GAS Tag 监听解耦) ---

	/** 是否正在游泳 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|State")
	bool bIsSwimming = false;

	/** 是否正在快速游泳 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|State")
	bool bIsFastSwimming = false;

	/** 游泳状态 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag SwimmingStateTag;

	/** 快速游泳状态 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag FastSwimmingStateTag;

	/** 游泳 Tag 变化回调 */
	UFUNCTION()
	void OnSwimmingTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** CMC 请求播放翻越蒙太奇的回调（从 VisualDataAsset 获取 ClimbUpMontage） */
	UFUNCTION()
	void OnClimbUpMontageRequested(UAnimMontage* MontageToPlay);

	/** ClimbUp 蒙太奇结束回调：调用 CMC::FinishClimbUp 恢复移动模式 */
	UFUNCTION()
	void OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
