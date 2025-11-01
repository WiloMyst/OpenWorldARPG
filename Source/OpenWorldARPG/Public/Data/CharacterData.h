// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/CharacterDataAsset.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "CharacterData.generated.h"

class UAS_Player;

/**
 * @struct FCharacterSaveData
 * @brief 用于封装从服务器或存档加载的角色持久化动态数据。
 */
USTRUCT(BlueprintType)
struct FCharacterSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FGameplayTag CharacterTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 CharacterLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 Experience = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 AscensionLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 ConstellationLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 FriendshipLevel = 1;

	/** 存储角色天赋等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TMap<FGameplayTag, int32> TalentLevels;

	/** 存储角色装备武器信息 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FName EquippedWeaponID;

	/** 存储角色装备圣遗物信息 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TArray<FName> EquippedArtifactIDs;
};


/**
 * @class ACharacterData
 * @brief 角色的“灵魂”和数据核心。
 * 这是一个Actor，用于在游戏运行时持久化存储单个游戏角色的所有数据，独立于其在世界中的物理表现(ACharacter)。
 * 它实现了IAbilitySystemInterface，是AbilitySystemComponent的真正所有者。
 */
UCLASS(BlueprintType, Blueprintable)
class OPENWORLDARPG_API ACharacterData : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACharacterData();

	// ======== 初始化函数 (核心API) ========
	/**
	 * @brief 初始化角色的核心数据，不依赖任何Actor。
	 * 由PlayerDataManager在加载存档时调用。
	 * @param InSaveData 从存档/服务器加载的动态数据。
	 * @param InDataAsset 角色的静态配置数据。
	 */
	UFUNCTION(BlueprintCallable, Category = "CharacterData|Initialization")
	void InitializeData(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset);

	// ======== IAbilitySystemInterface 核心实现 ========
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

	// ======== 公共Getters (蓝图可访问) ========

	/**
	 * @brief [蓝图可调用] 获取角色的DataSourceAsset。
	 * @return 返回角色的DataSourceAsset指针。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	UCharacterDataAsset* GetDataSourceAsset() const { return DataSourceAsset; }

	/**
	 * @brief [蓝图可调用] 获取角色的GameplayTag。
	 * @return 返回角色的GameplayTag。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	FGameplayTag GetCharacterTag() const { return CharacterTag; }

	/**
	 * @brief [蓝图可调用] 获取角色的武器子类。
	 * @return 返回武器的TSubclassOf。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	TSubclassOf<AWeaponBase> GetWeaponBlueprint() const { return DataSourceAsset ? DataSourceAsset->WeaponBlueprint : nullptr; }

	/**
	 * @brief [蓝图可调用] 获取角色的普通攻击蒙太奇。
	 * @return 返回角色的普通攻击蒙太奇数组。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	TArray<TSoftObjectPtr<UAnimMontage>> GetNormalAttackMontages() const { return DataSourceAsset ? DataSourceAsset->NormalAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }

	/**
	 * @brief [蓝图可调用] 获取角色的重击蒙太奇。
	 * @return 返回角色的重击蒙太奇数组。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	TArray<TSoftObjectPtr<UAnimMontage>> GetHeavyAttackMontages() const { return DataSourceAsset ? DataSourceAsset->HeavyAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }

	/**
	 * @brief [蓝图可调用] 获取角色的下落攻击蒙太奇。
	 * @return 返回角色的下落攻击蒙太奇数组。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	TArray<TSoftObjectPtr<UAnimMontage>> GetPlungeAttackMontages() const { return DataSourceAsset ? DataSourceAsset->PlungeAttack.Montages : TArray<TSoftObjectPtr<UAnimMontage>>(); }


	/**
	 * @brief [蓝图可调用] 获取角色的属性集。
	 * @return 返回角色的属性集指针。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	UAS_Player* GetAttributeSet() const { return AttributeSet; }

	/**
	 * @brief [蓝图可调用] 获取角色当前等级。
	 * @return 返回角色的当前等级。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	int32 GetCharacterLevel() const { return CharacterLevel; }

	/**
	 * @brief [蓝图可调用] 获取角色当前命之座等级。
	 * @return 返回角色的当前命之座等级。
	 */
	UFUNCTION(BlueprintPure, Category = "CharacterData")
	int32 GetConstellationLevel() const { return ConstellationLevel; }



protected:
	// ======== GAS核心组件 ========
	/** 角色的能力系统组件。管理所有技能、效果和属性。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 角色的属性集。存储生命、攻击等核心数值。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|GAS")
	TObjectPtr<UAS_Player> AttributeSet;

	// ======== 静态数据 (从DataAsset初始化而来，用于快速访问) ========
	/**
	 * @brief 保存对初始化时使用的源DataAsset的引用。
	 * 这非常有用，可以方便地在运行时查询角色的原始配置，而不需要重新加载。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Static")
	TObjectPtr<UCharacterDataAsset> DataSourceAsset;
	/** 角色GameplayTag */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Static")
	FGameplayTag CharacterTag;


	// ======== 动态数据 (由玩家存档决定) ========
	/** 角色当前等级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	int32 CharacterLevel = 1;

	/** 角色当前经验值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	int32 Experience = 0;

	/** 角色突破等级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	int32 AscensionLevel = 0;

	/** 角色命之座等级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	int32 ConstellationLevel = 0;

	/** 角色好感度等级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	int32 FriendshipLevel = 1;


	/** 天赋（技能）等级数组 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	TMap<FGameplayTag, int32> TalentLevels;

	// ======== 角色装备信息 ========

	/** 当前装备的武器ID */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	FName EquippedWeaponID;

	/** 当前装备的圣遗物ID列表 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterData|Dynamic")
	TArray<FName> EquippedArtifactIDs;
};