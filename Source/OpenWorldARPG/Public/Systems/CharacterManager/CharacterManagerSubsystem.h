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
 * 角色数据管理子系统（客户端缓存层）。
 *
 * 【职责变更】
 * 原本此子系统同时承担：
 *   1. 全局注册表查询（Tag→RowName）—— 已迁移至 UCharacterRegistrySubsystem (UGameInstanceSubsystem)
 *   2. per-玩家存档数据持有 —— 服务器侧已迁移至 UServerPlayerDataManager (GameState 组件)
 * 现在此子系统仅作为**客户端本地缓存**，供 UI 查询角色存档快照。
 *
 * 【数据来源】
 * - 单机/Listen Server：从 UInitialArchiveData 直接初始化（主机既是服务器也是客户端）
 * - Dedicated Server：不创建本子系统（ULocalPlayerSubsystem 在 DS 上不存在）
 * - 远程客户端：应通过服务器 RPC 同步存档数据到本缓存（当前未实现，见 [SYNC-TODO]）
 *
 * 【设计原则】
 * - 只读不写：UI 层只读不写，运行时数据反写由 APlayerCharacter::SyncAttributesToSaveData
 *   在服务器侧通过 UServerPlayerDataManager 完成
 * - 本地缓存：不参与网络复制，仅服务本地 UI 查询
 */
UCLASS()
class OPENWORLDARPG_API UCharacterManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UCharacterManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 注意：InitializeFromDataObject 仅用于单机/ListenServer 场景的模拟数据初始化。
	// [SYNC-TODO] 远程客户端应改为接收服务器 RPC 同步的存档数据。
	void InitializeFromDataObject(UObject* InDataObject);

	// --- SaveData 查询（客户端缓存） ---

	TArray<FCharacterSaveData> GetAllOwnedCharacterSaveData() const;
	const FCharacterSaveData* GetCharacterSaveData(const FGameplayTag& CharacterTag) const;
	int32 GetOwnedCharacterCount() const { return OwnedCharactersSaveData.Num(); }

	// --- SaveData 写入（客户端缓存） ---

	void SetCharacterSaveData(const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);

private:
	void EnsureTagMapBuilt() const {}

protected:
	/** 本地缓存的角色存档数据。单机/ListenServer 从 UInitialArchiveData 初始化。 */
	UPROPERTY()
	TMap<FGameplayTag, FCharacterSaveData> OwnedCharactersSaveData;
};
