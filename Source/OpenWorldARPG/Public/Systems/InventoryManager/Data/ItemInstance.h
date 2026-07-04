// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "ItemInstance.generated.h"

/**
 * FItemInstance - 物品动态实例数据 (瘦结构)
 *
 * 【架构重构：动静分离】
 * 此结构体仅保存运行时动态数据，不包含任何静态配置字段。
 * 静态字段 (ItemCategory, bIsStackable, ItemName, ItemIcon 等) 统一通过 ItemID
 * 查询 FItemData 获取，避免存档膨胀和数据冗余。
 *
 * 类型专属数据 (武器/圣遗物) 采用外键存储方案：
 * - 武器实例数据：通过 UInventoryManagerSubsystem::WeaponInstanceMap (TMap<FGuid, FWeaponInstanceData>) 查询
 * - 圣遗物实例数据：通过 UInventoryManagerSubsystem::ArtifactInstanceMap (TMap<FGuid, FArtifactInstanceData>) 查询
 * 这样普通材料/消耗品不会产生 FWeaponInstanceData/FArtifactInstanceData 的内存浪费。
 */
USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY()

    // --- 身份 ---

    /** 全局唯一标识 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGuid ItemGUID;

    /** 物品配置ID (外键，指向 DT_ItemDatabase 中的 FItemData 行) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ItemID = 0;

    // --- 可堆叠 ---

    /** 当前堆叠数量 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Count = 0;

    // --- 装备状态 ---

    /** 装备的角色ID (-1 表示未装备) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 EquippedCharacterID = -1;

    // --- 时间戳 ---

    /** 获取时间 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDateTime AcquiredTime;
};
