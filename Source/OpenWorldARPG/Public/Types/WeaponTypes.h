// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

// ======= 武器结构体 =======

USTRUCT(BlueprintType)
struct FWeaponInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 Level = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 AscensionLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 RefinementLevel = 1;
};
