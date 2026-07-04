// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtifactTypes.generated.h"

UENUM(BlueprintType)
enum class EArtifactSlot : uint8
{
    Flower      UMETA(DisplayName = "生之花"),
    Plume       UMETA(DisplayName = "死之羽"),
    Sands       UMETA(DisplayName = "时之沙"),
    Goblet      UMETA(DisplayName = "空之杯"),
    Circlet     UMETA(DisplayName = "理之冠"),
    None        UMETA(DisplayName = "无")
};

UENUM(BlueprintType)
enum class EArtifactStatType : uint8
{
    HP_Flat         UMETA(DisplayName = "生命值"),
    HP_Percent      UMETA(DisplayName = "生命值%"),
    ATK_Flat        UMETA(DisplayName = "攻击力"),
    ATK_Percent     UMETA(DisplayName = "攻击力%"),
    DEF_Flat        UMETA(DisplayName = "防御力"),
    DEF_Percent     UMETA(DisplayName = "防御力%"),
    CritRate        UMETA(DisplayName = "暴击率"),
    CritDamage      UMETA(DisplayName = "暴击伤害"),
    ElementalMastery UMETA(DisplayName = "元素精通"),
    EnergyRecharge  UMETA(DisplayName = "元素充能效率"),
    PhysDmgBonus    UMETA(DisplayName = "物理伤害加成"),
    None            UMETA(DisplayName = "无")
};

// --- 圣遗物结构体 ---

USTRUCT(BlueprintType)
struct FArtifactSubStat
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    EArtifactStatType StatType = EArtifactStatType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    float StatValue = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    int32 UpgradeCount = 0;
};

USTRUCT(BlueprintType)
struct FArtifactInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    int32 SetID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    EArtifactSlot Slot = EArtifactSlot::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    EArtifactStatType MainStat = EArtifactStatType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    float MainStatValue = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Artifact")
    TArray<FArtifactSubStat> SubStats;
};
