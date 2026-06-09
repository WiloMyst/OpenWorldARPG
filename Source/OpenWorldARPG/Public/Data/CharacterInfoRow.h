// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "CharacterInfoRow.generated.h"

class UCharacterDataAsset;

/**
 * DataTable (DT_CharacterInfo) 行结构。作为角色系统顶层索引，链接到 CharacterDataAsset。
 */
USTRUCT(BlueprintType)
struct FCharacterInfoRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    // --- 核心信息 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (DisplayName = "角色Tag"))
    FGameplayTag CharacterTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (DisplayName = "角色静态配置数据DA"))
    TObjectPtr<UCharacterDataAsset> CharacterDataAsset;

    // --- UI 快速显示 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色名称"))
    FText CharacterName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色头像"))
    TSoftObjectPtr<UTexture2D> HeadIcon;

    // --- 分类与筛选元数据 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色稀有度"))
    int32 Rarity;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色所属元素"))
    FGameplayTag ElementType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色武器类型"))
    FGameplayTag WeaponType;
};