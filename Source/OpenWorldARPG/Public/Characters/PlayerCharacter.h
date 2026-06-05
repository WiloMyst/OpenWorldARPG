// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Types/SharedTypes.h"
#include "PlayerCharacter.generated.h"

class UCharacterDataAsset;
class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UCharacterWeaponComponent;
class UAS_Player;
class UOpenWorldARPGCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMovementInput, float, InputX, float, InputY);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthUpdated);

/**
 * @class APlayerCharacter
 * @brief 玩家操控的实体角色基类。
 * 直接持有 ASC、AttributeSet，动态数据通过 FCharacterSaveData 唯一存储，无重复。
 */
UCLASS()
class OPENWORLDARPG_API APlayerCharacter : public AOpenWorldARPGCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaTime) override;

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
	// 输入处理接口
	// ==========================================

	/** * @brief 处理持续的移动输入
	 * @param InputX 对应蓝图 Action Value X (左右)
	 * @param InputY 对应蓝图 Action Value Y (前后)
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInput(float InputX, float InputY);

	/** * @brief 处理移动输入停止
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Input")
	void HandleMovementInputCompleted();

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
	TSubclassOf<class AWeaponBase> GetWeaponBlueprint() const;

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

	// ==========================================
	// 装备系统 (对标鸣潮：1武器 + 5圣遗物)
	// ==========================================

	/** 获取当前装备的武器 GUID */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Equipment")
	FGuid GetEquippedWeaponGUID() const { return EquippedWeaponGUID; }

	/** 获取指定槽位的圣遗物 GUID */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Equipment")
	FGuid GetEquippedArtifactGUID(EArtifactSlot Slot) const;

	/** 获取所有已装备圣遗物 GUID */
	UFUNCTION(BlueprintPure, Category = "PlayerCharacter|Equipment")
	const TMap<EArtifactSlot, FGuid>& GetEquippedArtifactGUIDs() const { return EquippedArtifactGUIDs; }

	/** 设置装备武器 (由 InventoryManagerSubsystem 调用) */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Equipment")
	void SetEquippedWeaponGUID(FGuid NewWeaponGUID) { EquippedWeaponGUID = NewWeaponGUID; }

	/** 设置指定槽位圣遗物 (由 InventoryManagerSubsystem 调用) */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Equipment")
	void SetEquippedArtifactGUID(EArtifactSlot Slot, FGuid NewArtifactGUID);

	/** 清除指定槽位圣遗物 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Equipment")
	void ClearEquippedArtifactGUID(EArtifactSlot Slot);

	const TArray<TSubclassOf<UGameplayAbility>>& GetPermanentAbilitiesToActivate() const { return PermanentAbilitiesToActivate; }

protected:
	/** 对应蓝图：常规移动 (基于视角的移动) */
	void NormalMovement(float InputX, float InputY);

	void OnMeshLoaded(const UCharacterDataAsset* DataAsset);
	virtual void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

	/** 处理跌落保护与重置 */
	void CheckKillZAndRespawn();

	/** 处理瞄准时的摄像机平滑过渡 */
	void AdjustAimingCamera(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|State")
	virtual void HandleDeath();

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
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
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

	// ==========================================
	// 配置项：跌落与重生 (告别 -4500 硬编码)
	// ==========================================

	/** 角色跌落重生的 Z 轴高度阈值 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|World")
	float KillZThreshold = -4500.0f;

	/** 重生时寻找的 PlayerStart 标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|World")
	FName RespawnPlayerStartTag = FName("PlayerStart");

	/** 重生时需要强制取消的技能 Tags (例如: Ability.Category.IsInAir) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerCharacter|Config|Tags")
	FGameplayTagContainer RespawnCancelAbilityTags;

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
	// 装备槽位 (GUID 引用 InventoryManagerSubsystem 中的物品实例)
	// ==========================================

	/** 当前装备的武器 GUID */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Equipment")
	FGuid EquippedWeaponGUID;

	/** 5个圣遗物槽位 (花/羽/沙/杯/头) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Equipment")
	TMap<EArtifactSlot, FGuid> EquippedArtifactGUIDs;
};
