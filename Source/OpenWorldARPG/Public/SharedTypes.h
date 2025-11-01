// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // 如果Struct要用于DataTable，必须包含这个
#include "SharedTypes.generated.h"


// ======= UENUM =======

// 游戏中最基础的物品类型枚举
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    None        UMETA(DisplayName = "无"),
    Weapon      UMETA(DisplayName = "武器"),
    Artifact    UMETA(DisplayName = "圣遗物"),
    Material    UMETA(DisplayName = "材料"),
    Food        UMETA(DisplayName = "食物"),
    QuestItem   UMETA(DisplayName = "任务")
};

// 物品稀有度枚举
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    IR_OneStar      UMETA(DisplayName = "★"),
    IR_TwoStar      UMETA(DisplayName = "★★"),
    IR_ThreeStar    UMETA(DisplayName = "★★★"),
    IR_FourStar     UMETA(DisplayName = "★★★★"),
    IR_FiveStar     UMETA(DisplayName = "★★★★★")
};

// 武器类型枚举，包含了游戏中所有的武器类型
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    Sword       UMETA(DisplayName = "单手剑"),
    Claymore    UMETA(DisplayName = "重刃"),
    Catalyst    UMETA(DisplayName = "法器"),
    Bow         UMETA(DisplayName = "弓"),
    Gun         UMETA(DisplayName = "铳")
};

// 武器的属性类型枚举
UENUM(BlueprintType)
enum class EWeaponSubStat : uint8
{
    None            UMETA(DisplayName = "无"),
    AttackPercent   UMETA(DisplayName = "攻击力百分比"),
    CritRate        UMETA(DisplayName = "暴击率"),
    CritDamage      UMETA(DisplayName = "暴击伤害"),
    EnergyRecharge  UMETA(DisplayName = "充能效率")
};


// ======= USTRUCT =======

// 所有物品数据的基础结构体
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    // == Base Information ==

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Base")
    int32 ItemID = -1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Base")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Base")
    EItemRarity Rarity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Base")
    EItemCategory Category;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Base", meta = (MultiLine = "true"))
    FText Description;

    // == Stacking Information ==

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Stacking")
    bool bIsStackable = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Stacking")
    int32 MaxStackSize = 99;

    // == Visuals ==

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Visuals")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Visuals")
    TSoftObjectPtr<UStaticMesh> ItemMesh;

    // == Usage ==

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data | Usage", meta = (MultiLine = "true"))
    FText UsageDescription;
};

// 武器的特定数据结构体
USTRUCT(BlueprintType)
struct FWeaponItemData : public FItemData
{
    GENERATED_BODY()

    // --- Core Combat Stats ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Combat")
    float BaseAttack = 0.0f; // 武器在1级时的基础攻击力

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Combat")
    EWeaponSubStat SubStatType = EWeaponSubStat::None; // 副词条的类型

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Combat")
    float SubStatInitialValue = 0.0f; // 副词条的初始数值

    // --- Classification & Visuals ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Classification")
    EWeaponType WeaponType = EWeaponType::Sword; // 武器的类型，用于决定动画和装备逻辑

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Classification")
    FName EquipSocketName; // 装备时附加到角色骨骼上的插槽名

    // --- Progression & Scaling ---

    // 引用一个曲线表资产，用于定义攻击力如何随等级成长。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Progression")
    TSoftObjectPtr<UCurveFloat> AttackGrowthCurve;

    // 引用另一个数据表，定义了这把武器在不同突破等级时需要的材料
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Progression")
    TSoftObjectPtr<UDataTable> AscensionMaterialsTable;

    // --- Special Abilities ---

    // (可选，高级用法) 引用一个GameplayAbility蓝图类，用于实现武器的被动效果
    // 这需要你的项目使用Gameplay Ability System (GAS)
    //UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Data | Abilities")
    //TSubclassOf<UGameplayAbility> PassiveAbilityClass;
};


// 背包分类标签页的数据结构体，用于UI显示
USTRUCT(BlueprintType)
struct FCategoryTabData : public FTableRowBase
{
    GENERATED_BODY()

    // 此标签页代表的实际物品分类。这是连接UI和数据的“关键钥匙”。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category Tab Data")
    EItemCategory TabCategory;

    // 标签页上要显示的分类名称
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category Tab Data")
    FText CategoryNameText;

    // 标签页上要显示的图标
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category Tab Data")
    TSoftObjectPtr<UTexture2D> CategoryIcon;
};