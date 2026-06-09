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

	/** 从 StartingRosterConfig 填充加载缓冲区，仅在游戏初始化时调用 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Initialization")
	void InitializeFromDataObject(UObject* InDataObject);

	/** 通过角色 Tag 查找 DT_CharacterInfo 中的完整数据行 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Data")
	const bool GetCharacterInfoRowByTag(const FGameplayTag& CharacterTag, FCharacterInfoRow& OutRow) const;

	/** 通过角色 Tag 查找对应的 DataTable 行名，找不到返回 NAME_None */
	UFUNCTION(BlueprintPure, Category = "CharacterManager|Data")
	FName GetRowNameByTag(const FGameplayTag& CharacterTag) const;

	/** 获取加载缓冲区（仅在 GeneratePlayerCharacters 期间使用）。 */
	const TArray<FCharacterSaveData>& GetLoadBuffer() const { return LoadBuffer; }

	/** 清空加载缓冲区（角色全部生成并初始化后调用）。 */
	void ClearLoadBuffer() { LoadBuffer.Empty(); }

	/** 从当前关卡的角色实例中收集存档数据。 */
	void CollectSaveDataFromCharacters(const TArray<APlayerCharacter*>& CharacterActors, TArray<FCharacterSaveData>& OutSaveData) const;

private:
	void PreloadAndProcessCharacterDataTable();

	/** 确保 TagToRowNameMap 已构建（延迟构建，首次访问时自动触发） */
	void EnsureTagMapBuilt() const;

protected:
	/** 初始化阶段暂存 StartingRosterConfig 数据的缓冲区，角色全部生成后应调用 ClearLoadBuffer() 清空 */
	UPROPERTY()
	TArray<FCharacterSaveData> LoadBuffer;

	/** 角色 Tag → DataTable 行名的映射表 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterManager|Config")
	TMap<FGameplayTag, FName> TagToRowNameMap;

	/** 标记 TagToRowNameMap 是否已成功构建，避免重复尝试 */
	mutable bool bTagMapBuilt = false;
};
