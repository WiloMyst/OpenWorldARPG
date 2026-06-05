// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "SharedTypes.generated.h"

// ======= UENUM (枚举) =======

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

// --- 圣遗物部位 ---
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

// --- 圣遗物主/副词条属性类型 ---
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

// --- 物品使用目标类型 ---
UENUM(BlueprintType)
enum class EItemUseTarget : uint8
{
    None            UMETA(DisplayName = "不可使用"),
    Self            UMETA(DisplayName = "自身"),
    SelectCharacter UMETA(DisplayName = "选择角色"),
    World           UMETA(DisplayName = "世界中使用")
};

// --- 物品排序模式 ---
UENUM(BlueprintType)
enum class EItemSortMode : uint8
{
    ByRarity        UMETA(DisplayName = "按稀有度"),
    ByLevel         UMETA(DisplayName = "按等级"),
    ByTime          UMETA(DisplayName = "按获取时间"),
    ByName          UMETA(DisplayName = "按名称")
};


// ======= USTRUCT (结构体) =======

// --- 圣遗物副词条 ---
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

// --- 武器实例数据 ---
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

// --- 圣遗物实例数据 ---
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

// --- 物品静态数据 (DataTable 行) ---
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

// --- 物品实例 (背包中的运行时数据) ---
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

// --- 背包分类标签页数据 ---
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
