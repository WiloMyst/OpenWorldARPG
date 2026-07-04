// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "Systems/InventoryManager/Types/ArtifactTypes.h"
#include "ItemData.generated.h"

/**
 * FItemData - 物品静态配置数据 (基础表)
 *
 * 【架构重构：关系型设计】
 * 此结构体仅保存所有物品通用的基础字段，作为 DT_ItemDatabase 的行结构。
 *
 * 对于武器和圣遗物，系统应使用 ItemID 作为外键，去 DT_WeaponData 或
 * DT_ArtifactData 专属数据表中查询特化属性，严禁在基础表中堆砌无关字段。
 * - 武器专属属性：查询 FWeaponStaticData (DT_WeaponData)
 * - 圣遗物专属属性：查询 FArtifactStaticData (DT_ArtifactData)
 */
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FText ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = "true"))
    FText ItemDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UTexture2D> ItemIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    bool bIsStackable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (EditCondition = "bIsStackable", ClampMin = "1"))
    int32 MaxStackSize = 9999;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemRarity ItemRarity = EItemRarity::Star1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemCategory ItemCategory = EItemCategory::Weapon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UStaticMesh> ItemMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = "true"))
    FText ItemFunctionDescription;

    // --- 使用/消耗系统 ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Use")
    EItemUseTarget UseTargetType = EItemUseTarget::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Use")
    FSoftObjectPath UseEffectClass;
};

/**
 * FWeaponStaticData - 武器专属静态数据
 *
 * 独立数据表 DT_WeaponData 的行结构。
 * 通过 ItemID 作为外键关联到 DT_ItemDatabase 中的 FItemData。
 * 严禁在基础 FItemData 表中堆砌武器专属字段。
 */
USTRUCT(BlueprintType)
struct FWeaponStaticData : public FTableRowBase
{
    GENERATED_BODY()

    /** 关联的基础物品ID (外键，指向 DT_ItemDatabase) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 ItemID = 0;

    /** 武器基础属性效果 (GE 软引用) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FSoftObjectPath WeaponBaseStatEffect;

    /** 武器突破效果 (Key: 突破等级, Value: GE 软引用) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TMap<int32, FSoftObjectPath> WeaponAscensionEffects;
};

/**
 * FArtifactStaticData - 圣遗物专属静态数据
 *
 * 独立数据表 DT_ArtifactData 的行结构。
 * 通过 ItemID 作为外键关联到 DT_ItemDatabase 中的 FItemData。
 * 严禁在基础 FItemData 表中堆砌圣遗物专属字段。
 */
USTRUCT(BlueprintType)
struct FArtifactStaticData : public FTableRowBase
{
    GENERATED_BODY()

    /** 关联的基础物品ID (外键，指向 DT_ItemDatabase) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    int32 ItemID = 0;

    /** 圣遗物套装ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    int32 ArtifactSetID = 0;

    /** 圣遗物部位 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    EArtifactSlot ArtifactSlot = EArtifactSlot::None;

    // --- 圣遗物套装效果 (2件套/4件套) ---

    /** 2件套效果 (GE 软引用) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact|SetBonus")
    FSoftObjectPath SetBonus2PieceEffect;

    /** 4件套效果 (GE 软引用) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact|SetBonus")
    FSoftObjectPath SetBonus4PieceEffect;
};
