// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "AbilitySystemInterface.h"
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
class UMovementStateMachineComponent;
class UBackpackComponent;
class UClimbingComponent;
class AWeaponBase;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMovementInput, float, InputX, float, InputY);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthUpdated);

/**
 * @class APlayerCharacter
 * @brief 玩家操控的实体角色基类。
 * 直接持有 ASC、AttributeSet，动态数据通过 FCharacterSaveData 唯一存储，无重复。
 *
 * 职责边界：
 * - 空间表现、物理位移、动画和技能执行
 * - 不持有装备/背包数据（由 EquipmentComponent 或 PlayerState 管理）
 * - 不在 Tick 中查询 GAS Tag（改为事件驱动）
 * - 不自行处理重生逻辑（由 FellOutOfWorld → HandleDeath 委托给 GameMode）
 */
UCLASS()
class OPENWORLDARPG_API APlayerCharacter : public AOpenWorldARPGCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaTime) override;

	/** 客户端收到 PlayerState 同步后，重新初始化 ASC 的 AbilityActorInfo */
	virtual void OnRep_PlayerState() override;

	/** 注册网络同步属性 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ==========================================
	// 核心初始化 API
	// ==========================================

	/**
	 * @brief 初始化角色的核心数据。
	 * @param InSaveData 从存档/配置加载的动态数据（移动到 RuntimeData，成为唯一数据源）。
	 * @param InDataAsset 角色的静态配置数据。
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Initialization")
	void InitializeCharacter(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset);

	/**
	 * @brief 设置角色的待机模式。
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|State")
	void SetStandbyMode(bool bNewStandbyState);

	// ==========================================
	// 输入处理接口 (邮局原则：Controller 只转发，Character 负责状态拦截与执行)
	// ==========================================

	/** 处理持续的移动输入 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInput(float InputX, float InputY);

	/** 处理移动输入停止 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInputCompleted();

	/** 处理交互输入 (拾取等)，内部查找 BackpackComponent 执行拾取 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleInteractInput();

	/** 处理跳跃开始输入，内部完成攀爬退出判定 + 发送 GAS 跳跃事件 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleJumpStartInput();

	/** 处理跳跃结束输入 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleJumpStopInput();

	/** 切换滑翔状态，内部完成 IsFalling 和 GlidingStateTag 的拦截判定 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void ToggleGlide();

	/** 切换瞄准状态，内部完成 AimingStateTag 的翻转判定 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void ToggleAim();

	// ==========================================
	// 角色切换流水线 (邮局原则：视觉表现归 Character，Possess 归 Controller)
	// ==========================================

	/**
	 * @brief 旧角色下场：保存 Transform、生成切换特效、进入待机。
	 * @param OutTransform [输出] 旧角色的世界变换，供新角色上场使用。
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void PerformSwapOut(FTransform& OutTransform);

	/**
	 * @brief 新角色上场：设置 Transform、解除待机。
	 * @param InTransform 旧角色留下的世界变换。
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Swap")
	void PerformSwapIn(const FTransform& InTransform);

	/** 多播：在所有客户端上生成角色切换特效 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnSwapFX(FVector Location);

	/** 多播：同步待机模式的状态变更到所有客户端 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetStandbyMode(bool bNewStandbyState);

	/** 多播：同步元素/武器类型 Tag 到客户端（AddLooseGameplayTag 不同步） */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_AddLooseGameplayTags(FGameplayTag ElementTypeTag, FGameplayTag WeaponTypeTag);

	/** 实际执行待机模式状态变更（由 SetStandbyMode 和 Multicast 回调调用） */
	void ApplyStandbyMode(bool bNewStandbyState);

	/**
	 * @brief 检查当前角色是否允许被切换下场。
	 * 内部完成运动模式和 GAS 状态标签的拦截判定。
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Swap")
	bool CanSwapOut() const;

	/**
	 * @brief 检查目标角色是否允许被切换上场。
	 * 内部完成 GAS 状态标签的拦截判定。
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Swap")
	bool CanSwapIn() const;

	// ==========================================
	// 动画与物理控制 API
	// ==========================================

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupBaseBehaviorAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupAimAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void SetupPhysicsAnimLayers();

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
	void ClearPhysicsAnimLayers();

	// ==========================================
	// 越界处理 (替代 Tick 中的 CheckKillZAndRespawn)
	// ==========================================

	/** 引擎原生越界回调，替代 Tick 中每帧检测 Z 坐标 */
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	// ==========================================
	// 公共 Getters
	// ==========================================

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Weapon")
	USceneComponent* GetWeaponRestSocket() const { return WeaponRestSocket; }

	/** 获取 ASC 指针，实现 IAbilitySystemInterface */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|GAS")
	UAS_Player* GetAttributeSet() const { return AttributeSet; }

	/** 获取自定义移动组件 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Movement")
	UOpenWorldARPGCharacterMovementComponent* GetCustomMovementComp() const;

	/** 获取移动状态机组件 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Movement")
	UMovementStateMachineComponent* GetMovementStateMachine() const;

	/** 获取静态配置数据资产 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	UCharacterDataAsset* GetDataSourceAsset() const { return DataSourceAsset; }

	/** 获取角色 GameplayTag (从 RuntimeData 读取) */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetCharacterTag() const;

	/** 获取待机状态 Tag (从蓝图默认值配置) */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	FGameplayTag GetStandbyStateTag() const { return StandbyStateTag; }

	/** 获取武器蓝图 (从 DataSourceAsset 读取) */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TSubclassOf<AWeaponBase> GetWeaponBlueprint() const;

	/** 获取普通攻击蒙太奇 (从 DataSourceAsset 读取) */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetNormalAttackMontages() const;

	/** 获取重击蒙太奇 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetHeavyAttackMontages() const;

	/** 获取下落攻击蒙太奇 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	TArray<TSoftObjectPtr<UAnimMontage>> GetPlungeAttackMontages() const;

	/** 获取角色等级 (从 RuntimeData 读取) */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	int32 GetCharacterLevel() const;

	/** 获取命之座等级 */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Data")
	int32 GetConstellationLevel() const;

	/** 获取运行时数据的可变引用 (用于存档回写等) */
	FCharacterSaveData& GetRuntimeDataRef() { return RuntimeData; }

	/** 获取运行时数据的只读引用 */
	const FCharacterSaveData& GetRuntimeData() const { return RuntimeData; }

	const TArray<TSubclassOf<UGameplayAbility>>& GetPermanentAbilitiesToActivate() const { return PermanentAbilitiesToActivate; }

protected:
	/** 对应蓝图：常规移动 (基于视角的移动) */
	void NormalMovement(float InputX, float InputY);

	void OnMeshLoaded(const UCharacterDataAsset* DataAsset);
	virtual void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

	/** 处理瞄准时的摄像机平滑过渡 (Tick 中只做插值，不查 GAS Tag) */
	void AdjustAimingCamera(float DeltaTime);

	/** 瞄准状态 Tag 变化回调 (事件驱动，替代 Tick 中查询) */
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
	// ==========================================
	// 核心组件
	// ==========================================

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

	/** 移动状态机组件 (FSM) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Movement")
	TObjectPtr<UMovementStateMachineComponent> MovementStateMachine;

	// ==========================================
	// 输入缓存变量
	// ==========================================

	/** 缓存的 X 轴输入值 (可供动画蓝图读取) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputX = 0.0f;

	/** 缓存的 Y 轴输入值 (可供动画蓝图读取) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Input")
	float CurrentInputY = 0.0f;

	// ==========================================
	// GAS 核心组件
	// ==========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|GAS")
	TObjectPtr<UAS_Player> AttributeSet;

	// ==========================================
	// 数据层 (唯一数据源)
	// ==========================================

	/** 静态配置 (从 DataAsset 初始化，不随存档变化) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
	TObjectPtr<UCharacterDataAsset> DataSourceAsset;

	/**
	 * 动态数据 (唯一数据源)。
	 * 初始化时从存档/配置移入，运行时直接修改此结构体，存档时直接序列化。
	 * 不再在 Actor 上重复声明 CharacterLevel 等字段。
	 * Replicated：客户端需要读取 CharacterTag、CharacterLevel 等数据驱动 UI。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "PlayerCharacter|Data")
	FCharacterSaveData RuntimeData;

	// ==========================================
	// 运行时缓存
	// ==========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Runtime")
	TArray<TSubclassOf<UGameplayAbility>> PermanentAbilitiesToActivate;

	/** 进入 Standby 前正在 Tick 的组件快照，用于精确恢复 */
	TArray<TWeakObjectPtr<UActorComponent>> TickingComponentsSnapshot;

	// ==========================================
	// 配置项
	// ==========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag StandbyStateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag DeathAbilityTag;

	/** 重生时需要强制取消的技能 Tags (例如: Ability.Category.IsInAir) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTagContainer RespawnCancelAbilityTags;

	// ==========================================
	// 配置项：角色切换 (从 Controller 迁移，视觉表现归 Character)
	// ==========================================

	/** 切换特效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TObjectPtr<UNiagaraSystem> CharacterSwapFX;

	/** 特效生成位置偏移 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

	/** 特效缩放 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

	/** 目标角色禁止切换上场的状态 Tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	FGameplayTagContainer PreventSwitchTags;

	/** 允许切换下场的运动模式 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Swap")
	TArray<TEnumAsByte<EMovementMode>> AllowedSwapOutMovementModes;

	// ==========================================
	// 配置项：输入事件 Tags (由 Controller 通过 GAS 事件发送)
	// ==========================================

	/** 标识角色处于不可控状态的 Tag (用于 HandleMovementInput 内部拦截) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag UncontrollableStateTag;

	/** 标识角色处于攀爬状态的 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag ClimbingStateTag;

	/** 标识角色处于滑翔状态的 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlidingStateTag;

	/** 跳跃开始事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStartEventTag;

	/** 跳跃结束事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag JumpStopEventTag;

	/** 滑翔开始事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStartEventTag;

	/** 滑翔结束事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag GlideStopEventTag;

	/** 瞄准开始事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStartEventTag;

	/** 瞄准结束事件 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimStopEventTag;

	// ==========================================
	// 配置项：瞄准与摄像机 (彻底数据驱动)
	// ==========================================

	/** 标识角色处于瞄准状态的 Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTag AimingStateTag;

	/** 摄像机插值过渡速度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float CameraInterpSpeed = 10.0f;

	/** 正常状态下的摄像机臂长 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float NormalTargetArmLength = 400.0f;

	/** 瞄准状态下的摄像机臂长 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	float AimingTargetArmLength = 150.0f;

	/** 正常状态下的摄像机偏移 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector NormalSocketOffset = FVector::ZeroVector;

	/** 瞄准状态下的摄像机偏移 (例如右移并上抬) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Camera")
	FVector AimingSocketOffset = FVector(0.0f, 50.0f, 20.0f);

	// ==========================================
	// 摄像机运行时状态 (事件驱动，不在 Tick 中查 GAS)
	// ==========================================

	/** 当前目标臂长 (由 OnAimingTagChanged 事件设置，Tick 只做插值) */
	float CurrentTargetArmLength = 400.0f;

	/** 当前目标偏移 (由 OnAimingTagChanged 事件设置，Tick 只做插值) */
	FVector CurrentTargetSocketOffset = FVector::ZeroVector;
};
