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
 * 项目全局资产配置中心。所有资产路径统一在此管理。
 * 编辑器 → 项目设置 → OpenWorldARPG 中集中配置。
 */
UCLASS(DefaultConfig, Config=Game, meta=(DisplayName="OpenWorldARPG"))
class OPENWORLDARPG_API UOpenWorldARPGSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UOpenWorldARPGSettings& Get() { return *GetDefault<UOpenWorldARPGSettings>(); }

	// --- 数据表 ---
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="角色信息数据表"))
	TSoftObjectPtr<UDataTable> CharacterInfoTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="物品数据库数据表"))
	TSoftObjectPtr<UDataTable> ItemDatabaseTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Data Tables", meta=(ToolTip="背包分类标签页数据表"))
	TSoftObjectPtr<UDataTable> InventoryCategoryTabDataTable;

	// --- 角色通用配置 ---
	UPROPERTY(Config, EditDefaultsOnly, Category = "Character", meta=(ToolTip="角色通用技能数据资产"))
	TSoftObjectPtr<UCharacterGeneralDataAsset> PlayerCharacterGeneralAbilityDataAsset;

	// --- UI ---
	UPROPERTY(Config, EditDefaultsOnly, Category = "UI", meta=(ToolTip="UI映射数据资产"))
	TSoftObjectPtr<UUIDataAsset> UIMapDataAsset;

	UPROPERTY(Config, EditDefaultsOnly, Category = "UI", meta=(ToolTip="加载界面Widget类"))
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

#if WITH_EDITOR
	virtual FName GetCategoryName() const override;
#endif
};
