// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ItemTypes.h"
#include "Types/WeaponTypes.h"
#include "Types/ArtifactTypes.h"
#include "ItemInstance.generated.h"

// ======= 物品实例 (背包中的运行时数据) =======

USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY()

    // === 身份 ===

    /** 全局唯一标识符，用于跨网络/存档/装备引用 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGuid ItemGUID;

    /** 物品静态ID (对应 DataTable 行) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ItemID = 0;

    /** 物品分类 (从 FItemData 缓存，避免频繁查表) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    EItemCategory ItemCategory = EItemCategory::Weapon;

    /** 是否为可堆叠物品 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bIsStackable = false;

    // === 可堆叠 ===

    /** 数量 (可堆叠物品: 1~MaxStackSize; 不可堆叠物品: 固定为1) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Count = 0;

    // === 装备状态 ===

    /** 装备目标角色ID (-1 = 未装备) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 EquippedCharacterID = -1;

    // === 类型专属数据 ===

    /** 武器实例数据 (仅武器使用) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Weapon")
    FWeaponInstanceData WeaponData;

    /** 圣遗物实例数据 (仅圣遗物使用) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Artifact")
    FArtifactInstanceData ArtifactData;

    // === 时间戳 ===

    /** 获取时间 (用于排序) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDateTime AcquiredTime;
};
