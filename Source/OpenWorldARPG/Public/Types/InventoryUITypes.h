// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Types/ItemTypes.h"
#include "InventoryUITypes.generated.h"

// ======= 背包分类标签页数据 =======

USTRUCT(BlueprintType)
struct FInventoryCategoryTabData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    FText CategoryNameText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    TSoftObjectPtr<UTexture2D> CategoryIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    EItemCategory TabCategory = EItemCategory::Weapon;
};
