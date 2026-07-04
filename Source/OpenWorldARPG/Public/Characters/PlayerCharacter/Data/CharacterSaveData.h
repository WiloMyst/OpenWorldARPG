// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CharacterSaveData.generated.h"

/**
 * 天赋等级键值对。TArray 替代 TMap 以支持网络复制。
 */
USTRUCT(BlueprintType)
struct FTalentLevelEntry
{
	GENERATED_BODY()

	/** 天赋标签 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FGameplayTag TalentTag;

	/** 天赋等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 Level = 0;
};

/**
 * 角色持久化动态数据。APlayerCharacter::RuntimeData 的唯一数据源。
 */
USTRUCT(BlueprintType)
struct FCharacterSaveData
{
	GENERATED_BODY()

	/** 角色标签 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FGameplayTag CharacterTag;

	/** 角色等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 CharacterLevel = 1;

	/** 经验值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 Experience = 0;

	/** 升星等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 AscensionLevel = 0;

	/** 星座等级 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	int32 ConstellationLevel = 0;

	/** 天赋等级列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TArray<FTalentLevelEntry> TalentLevels;

	/** 已装备武器ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FName EquippedWeaponID;

	/** 已装备装备ID列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TArray<FName> EquippedArtifactIDs;

	// --- UI 展示面板属性快照 (Attribute Snapshots) ---
	// 【设计原则】：UI 面板展示的唯一数据源 (Source of Truth)。
	// 后台（未上阵）角色没有实例化 Actor，无法通过 GAS 获取属性，
	// 因此角色在场景存活时由 APlayerCharacter::SyncAttributesToSaveData 反写回快照，
	// UI 面板直接读取这些快照字段，避免与 GAS 强耦合。

	/** 最大生命值快照 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData|Snapshot")
	float MaxHealth = 1000.0f;

	/** 攻击力快照 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData|Snapshot")
	float Attack = 100.0f;

	/** 防御力快照 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData|Snapshot")
	float Defense = 50.0f;

	/** 暴击率快照 (百分比，如 0.05 代表 5%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData|Snapshot")
	float CritRate = 0.05f;

	/** 暴击伤害快照 (百分比，如 0.5 代表 50%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData|Snapshot")
	float CritDamage = 0.50f;
};
