// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "ItemData.generated.h"

/**
 * FItemData - 物品静态配置数据 (基础表)
 *
 * 此结构体保存所有物品通用的基础字段，作为 DT_ItemDataInfo 的行结构。
 * 业务范围收敛为拾取/丢弃：字段仅覆盖展示与背包规则 (堆叠/分类)。
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
};
