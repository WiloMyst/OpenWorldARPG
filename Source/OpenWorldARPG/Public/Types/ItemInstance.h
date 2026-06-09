// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ItemTypes.h"
#include "Types/WeaponTypes.h"
#include "Types/ArtifactTypes.h"
#include "ItemInstance.generated.h"

USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY()

    // --- 身份 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGuid ItemGUID;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ItemID = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    EItemCategory ItemCategory = EItemCategory::Weapon;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bIsStackable = false;

    // --- 可堆叠 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Count = 0;

    // --- 装备状态 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 EquippedCharacterID = -1;

    // --- 类型专属数据 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Weapon")
    FWeaponInstanceData WeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Artifact")
    FArtifactInstanceData ArtifactData;

    // --- 时间戳 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDateTime AcquiredTime;
};
