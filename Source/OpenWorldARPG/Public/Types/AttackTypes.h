// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttackTypes.generated.h"

// ======= 攻击通用枚举 =======

UENUM(BlueprintType)
enum class EAttackType : uint8
{
    Normal      UMETA(DisplayName = "普通攻击"),
    Heavy       UMETA(DisplayName = "重击"),
    Plunge      UMETA(DisplayName = "下落攻击")
};