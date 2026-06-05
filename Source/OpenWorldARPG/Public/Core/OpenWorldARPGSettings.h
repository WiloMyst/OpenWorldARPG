// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenWorldARPGSettings.generated.h"

class UDataTable;
class UUIDataAsset;
class UCharacterGeneralDataAsset;
class ULoadingScreenWidget;

/**
 * @class UOpenWorldARPGSettings
 * @brief 项目全局资产配置中心。
 * 所有原本硬编码在 C++ 中的资产路径，统一迁移到这里。
 * 策划/开发者可以在 编辑器 → 项目设置 → OpenWorldARPG 中集中管理。
 *
 * 使用方式：
 *   const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
 *   UDataTable* Table = Settings.CharacterInfoTable.LoadSynchronous();
 */
UCLASS(DefaultConfig, Config=Game, meta=(DisplayName="OpenWorldARPG"))
class OPENWORLDARPG_API UOpenWorldARPGSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 全局访问入口 */
	static const UOpenWorldARPGSettings& Get() { return *GetDefault<UOpenWorldARPGSettings>(); }

	// ==========================================
	// 数据表
	// ==========================================

	/** 角色信息数据表 (DT_CharacterInfo) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="角色信息数据表"))
	TSoftObjectPtr<UDataTable> CharacterInfoTable;

	/** 物品数据库数据表 (DT_ItemDatabase) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="物品数据库数据表"))
	TSoftObjectPtr<UDataTable> ItemDatabaseTable;

	/** 背包分类标签页数据表 (DT_InventoryCategoryTabData) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="背包分类标签页数据表"))
	TSoftObjectPtr<UDataTable> InventoryCategoryTabDataTable;

	// ==========================================
	// 角色通用配置
	// ==========================================

	/** 角色通用技能数据资产 (DA_CharacterGeneral) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Character", meta=(ToolTip="角色通用技能数据资产"))
	TSoftObjectPtr<UCharacterGeneralDataAsset> PlayerCharacterGeneralAbilityDataAsset;

	// ==========================================
	// UI
	// ==========================================

	/** UI 映射数据资产 (DA_UIMap) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "UI", meta=(ToolTip="UI映射数据资产"))
	TSoftObjectPtr<UUIDataAsset> UIMapDataAsset;

	/** 加载界面 Widget 类 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "UI", meta=(ToolTip="加载界面Widget类"))
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

#if WITH_EDITOR
	virtual FName GetCategoryName() const override;
#endif
};
