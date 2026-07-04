// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "CharacterManagerSubsystem.generated.h"

class UObject;
class UInitialArchiveData;
class UDataTable;
class APlayerCharacter;
struct FCharacterRegistryRow;


/**
 * 角色数据管理子系统（纯数据层）。
 * 持久化存储玩家拥有的角色存档数据，按 Tag 查询。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UCharacterManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void InitializeFromDataObject(UObject* InDataObject);

	// --- Registry 查询 ---

	bool GetCharacterRegistryRowByTag(const FGameplayTag& CharacterTag, FCharacterRegistryRow& OutRow) const;
	FName GetRowNameByTag(const FGameplayTag& CharacterTag) const;

	// --- SaveData 查询 ---

	TArray<FCharacterSaveData> GetAllOwnedCharacterSaveData() const;
	const FCharacterSaveData* GetCharacterSaveData(const FGameplayTag& CharacterTag) const;
	int32 GetOwnedCharacterCount() const { return OwnedCharactersSaveData.Num(); }

	// --- SaveData 写入 ---

	void SetCharacterSaveData(const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);
	void CollectSaveDataFromCharacters(const TArray<APlayerCharacter*>& CharacterActors, TArray<FCharacterSaveData>& OutSaveData) const;

private:
	void PreloadAndProcessCharacterDataTable();
	void EnsureTagMapBuilt() const;

protected:
	UPROPERTY()
	TMap<FGameplayTag, FCharacterSaveData> OwnedCharactersSaveData;

	UPROPERTY()
	TMap<FGameplayTag, FName> TagToRowNameMap;

	mutable bool bTagMapBuilt = false;
};
