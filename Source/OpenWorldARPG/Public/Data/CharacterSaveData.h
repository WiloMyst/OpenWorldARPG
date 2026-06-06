// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CharacterSaveData.generated.h"

/**
 * @struct FCharacterSaveData
 * @brief 角色持久化动态数据，作为 APlayerCharacter::RuntimeData 的唯一数据源。
 * 同时用于存档序列化和菜单数据访问。
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TMap<FGameplayTag, int32> TalentLevels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FName EquippedWeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	TArray<FName> EquippedArtifactIDs;
};
