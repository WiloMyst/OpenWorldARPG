// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Weapon      UMETA(DisplayName = "武器"),
    Artifact    UMETA(DisplayName = "圣遗物"),
    Material    UMETA(DisplayName = "材料"),
    Food        UMETA(DisplayName = "食物"),
    Quest       UMETA(DisplayName = "任务")
};

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Star1       UMETA(DisplayName = "★"),
    Star2       UMETA(DisplayName = "★★"),
    Star3       UMETA(DisplayName = "★★★"),
    Star4       UMETA(DisplayName = "★★★★"),
    Star5       UMETA(DisplayName = "★★★★★")
};

UENUM(BlueprintType)
enum class EItemUseTarget : uint8
{
    None            UMETA(DisplayName = "不可使用"),
    Self            UMETA(DisplayName = "自身"),
    SelectCharacter UMETA(DisplayName = "选择角色"),
    World           UMETA(DisplayName = "世界中使用")
};

UENUM(BlueprintType)
enum class EItemSortMode : uint8
{
    ByRarity        UMETA(DisplayName = "按稀有度"),
    ByLevel         UMETA(DisplayName = "按等级"),
    ByTime          UMETA(DisplayName = "按获取时间"),
    ByName          UMETA(DisplayName = "按名称")
};
