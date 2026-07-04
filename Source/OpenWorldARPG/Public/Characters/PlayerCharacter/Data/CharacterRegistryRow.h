// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "CharacterRegistryRow.generated.h"

class UCharacterVisualDataAsset;
class UCharacterCombatDataAsset;

/**
 * 角色全局注册表行结构。作为 DataTable (DT_CharacterRegistry) 的行定义。
 *
 * 【架构设计：极轻量全局索引 + SSOT】
 * 本结构体是整个角色系统的顶层索引，职责：
 * 1. 作为 UI 展示数据的唯一真相源 (Single Source of Truth)
 *    - 角色名称、头像、稀有度等 UI 元数据只在此处配置
 *    - UCharacterVisualDataAsset 和 UCharacterCombatDataAsset 中不再冗余这些字段
 * 2. 作为表现层和战斗层的桥梁
 *    - 通过 TSoftObjectPtr 引用 VisualData 和 CombatData
 *    - 运行时由 GameAssetManagerSubsystem 按需异步加载，不产生硬引用
 *
 * 【极致内存管理：全软引用桥梁】
 * VisualData 和 CombatData 均使用 TSoftObjectPtr（而非 TObjectPtr），
 * 这意味着本行结构常驻内存时不会连带加载任何 DataAsset。
 * 只有当角色被选入队伍时，GameAssetManagerSubsystem 才会解析这些软引用，
 * 将对应的 DataAsset 异步加载到内存中。
 *
 * 【三层解耦：互不硬引用】
 * - FCharacterRegistryRow：UI 元数据 + 软引用桥梁 → 策划A负责
 * - UCharacterVisualDataAsset：外观/动画数据 → 美术负责
 * - UCharacterCombatDataAsset：战斗/天赋数据 → 战斗策划负责
 * 三层资产独立签出，Perforce 不会互相锁死。
 * 三个结构之间不存在任何 TObjectPtr 硬引用，只有 TSoftObjectPtr 软引用。
 */
USTRUCT(BlueprintType)
struct FCharacterRegistryRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    // --- 核心索引 ---

    /** 角色唯一标识 Tag（如 Character.WiloMyst） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core", meta = (DisplayName = "角色Tag"))
    FGameplayTag CharacterTag;

    // --- UI 快速展示元数据（SSOT：唯一真相源） ---
    // 以下字段是 UI 系统展示角色信息的唯一数据来源。
    // UCharacterVisualDataAsset 和 UCharacterCombatDataAsset 中不再冗余存储这些字段。

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色名称"))
    FText CharacterName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色头衔"))
    FText CharacterTitle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色头像"))
    TSoftObjectPtr<UTexture2D> HeadIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "角色立绘"))
    TSoftObjectPtr<UTexture2D> SplashArt;

    // --- 分类与筛选元数据 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色稀有度"))
    int32 Rarity = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色所属元素"))
    FGameplayTag ElementType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Metadata", meta = (DisplayName = "角色武器类型"))
    FGameplayTag WeaponType;

    // --- 核心桥梁：软引用指向表现层和战斗层 ---

    /**
     * 角色外观表现数据资产（软引用）。
     * 包含骨骼网格体、动画蓝图、武器蓝图、攀爬蒙太奇等纯视觉数据。
     * 运行时由 GameAssetManagerSubsystem 按需异步加载。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bridge", meta = (DisplayName = "外观表现数据资产"))
    TSoftObjectPtr<UCharacterVisualDataAsset> VisualData;

    /**
     * 角色战斗逻辑数据资产（软引用）。
     * 包含属性 GE、突破增益、天赋技能字典等纯战斗数据。
     * 运行时由 GameAssetManagerSubsystem 按需异步加载。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bridge", meta = (DisplayName = "战斗逻辑数据资产"))
    TSoftObjectPtr<UCharacterCombatDataAsset> CombatData;
};
