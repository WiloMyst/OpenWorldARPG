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
};
