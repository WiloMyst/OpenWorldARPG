// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Types/ItemTypes.h"
#include "Types/ArtifactTypes.h"
#include "Types/WeaponTypes.h"
#include "ItemData.generated.h"

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

    // --- 武器专属 ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Weapon")
    FSoftObjectPath WeaponBaseStatEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Weapon")
    TMap<int32, FSoftObjectPath> WeaponAscensionEffects;

    // --- 圣遗物专属 ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Artifact")
    int32 ArtifactSetID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Artifact")
    EArtifactSlot ArtifactSlot = EArtifactSlot::None;

    // --- 圣遗物套装效果 (2件套/4件套) ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Artifact")
    FSoftObjectPath SetBonus2PieceEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data|Artifact")
    FSoftObjectPath SetBonus4PieceEffect;
};
