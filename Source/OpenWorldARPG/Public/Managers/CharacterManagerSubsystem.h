// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterSaveData.h"
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
class OPENWORLDARPG_API UCharacterManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCharacterManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 从 UInitialArchiveData 填充玩家拥有的角色存档数据 */
	void InitializeFromDataObject(UObject* InDataObject);

	// --- Registry 查询 ---

	bool GetCharacterRegistryRowByTag(const FGameplayTag& CharacterTag, FCharacterRegistryRow& OutRow) const;
	FName GetRowNameByTag(const FGameplayTag& CharacterTag) const;

	// --- SaveData 查询 ---

	/** 获取所有已拥有角色的存档数据（拷贝） */
	TArray<FCharacterSaveData> GetAllOwnedCharacterSaveData() const;

	/** 通过 Tag 获取存档数据只读指针，找不到返回 nullptr */
	const FCharacterSaveData* GetCharacterSaveData(const FGameplayTag& CharacterTag) const;

	/** 已拥有角色数量 */
	int32 GetOwnedCharacterCount() const { return OwnedCharactersSaveData.Num(); }

	// --- SaveData 写入 ---

	/** 替换指定角色的存档数据（Tag 不存在则不操作） */
	void SetCharacterSaveData(const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);

	/** 从关卡角色实例收集存档数据 */
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
