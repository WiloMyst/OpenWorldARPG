// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterSaveData.h"
#include "CharacterManagerSubsystem.generated.h"

class UObject;
class UStartingRosterConfig;
class UDataTable;
class APlayerCharacter;
struct FCharacterInfoRow;


/**
 * 角色数据管理子系统（纯数据层，不持有 Actor 引用）。
 * 持久化存储玩家拥有的所有角色存档数据，按需查询，不再一次性加载全部角色实体。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCharacterManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 核心 API ---

	/** 从 StartingRosterConfig 填充玩家拥有的角色存档数据，仅在游戏初始化时调用 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Initialization")
	void InitializeFromDataObject(UObject* InDataObject);

	/** 通过角色 Tag 查找 DT_CharacterInfo 中的完整数据行 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Data")
	const bool GetCharacterInfoRowByTag(const FGameplayTag& CharacterTag, FCharacterInfoRow& OutRow) const;

	/** 通过角色 Tag 查找对应的 DataTable 行名，找不到返回 NAME_None */
	UFUNCTION(BlueprintPure, Category = "CharacterManager|Data")
	FName GetRowNameByTag(const FGameplayTag& CharacterTag) const;

	// --- 存档数据 API ---

	/** 通过角色 Tag 获取对应的存档数据。找到返回 true，否则 false */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|SaveData")
	bool GetCharacterSaveData(const FGameplayTag& CharacterTag, FCharacterSaveData& OutData) const;

	/** 更新指定角色的存档数据（用于存档同步、运行时属性变更等）。Tag 不存在则不操作 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|SaveData")
	void UpdateCharacterSaveData(const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);

	/** 获取玩家拥有的角色总数 */
	UFUNCTION(BlueprintPure, Category = "CharacterManager|SaveData")
	int32 GetOwnedCharacterCount() const { return OwnedCharactersSaveData.Num(); }

	/** 从当前关卡的角色实例中收集存档数据 */
	void CollectSaveDataFromCharacters(const TArray<APlayerCharacter*>& CharacterActors, TArray<FCharacterSaveData>& OutSaveData) const;

private:
	void PreloadAndProcessCharacterDataTable();

	/** 确保 TagToRowNameMap 已构建（延迟构建，首次访问时自动触发） */
	void EnsureTagMapBuilt() const;

protected:
	/** 玩家拥有的所有角色存档数据，以 CharacterTag 为键持久化存储。
	 *  替代原先的 LoadBuffer：不再一次性加载全部角色实体，
	 *  仅在需要生成队伍角色时按 Tag 查询对应的 SaveData。 */
	UPROPERTY()
	TMap<FGameplayTag, FCharacterSaveData> OwnedCharactersSaveData;

	/** 角色 Tag → DataTable 行名的映射表 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterManager|Config")
	TMap<FGameplayTag, FName> TagToRowNameMap;

	/** 标记 TagToRowNameMap 是否已成功构建，避免重复尝试 */
	mutable bool bTagMapBuilt = false;
};
