// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "CharacterInfoRow.generated.h"

class UCharacterDataAsset;

/**
 * @struct FCharacterInfoRow
 * @brief DataTable (DT_CharacterInfo) 中每一行的结构定义。
 * 这个结构体非常轻量，主要用于：
 * 1. 作为角色系统的顶层索引，通过RowName（角色ID）查找。
 * 2. 链接到包含角色所有详细配置的 UCharacterDataAsset。
 * 3. 存储一些需要在表格中进行快速浏览、排序或筛选的元数据（如稀有度、元素类型）。
 */
USTRUCT(BlueprintType)
struct FCharacterInfoRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    // ======== 核心信息 ========

    /**
     * @brief 角色唯一的GameplayTag标识。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (DisplayName = "角色Tag"))
    FGameplayTag CharacterTag;

    /**
     * @brief 指向定义该角色所有详细配置的DataAsset。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (DisplayName = "角色静态配置数据DA"))
    TObjectPtr<UCharacterDataAsset> CharacterDataAsset;


    // ======== UI快速显示信息 ========

    /**
     * @brief 角色的名字。
     * 用于UI快速显示。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色名称"))
    FText CharacterName;

    /**
     * @brief 角色的头像图标。
     * 用于在角色选择界面、队伍配置界面等地方快速显示。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色头像"))
    TSoftObjectPtr<UTexture2D> HeadIcon;


    // ======== DataTable分类与筛选元数据 ========

    /**
     * @brief 角色稀有度 (例如: 4 或 5)。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色稀有度"))
    int32 Rarity;

    /**
     * @brief 角色所属元素 (例如: "Element.Electro", "Element.Geo")。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色所属元素"))
    FGameplayTag ElementType;

    /**
     * @brief 角色所属武器类型 (例如: "Weapon.Sword", "Weapon.Polearm")。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色武器类型"))
    FGameplayTag WeaponType;
};