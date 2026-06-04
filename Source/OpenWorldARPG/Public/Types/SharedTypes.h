// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" 
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "SharedTypes.generated.h"

// ======= UENUM (枚举) =======

// 对应蓝图：E_ItemCategory (物品类型)
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Weapon      UMETA(DisplayName = "武器"),
    Artifact    UMETA(DisplayName = "圣遗物"),
    Material    UMETA(DisplayName = "材料"),
    Food        UMETA(DisplayName = "食物"),
    Quest       UMETA(DisplayName = "任务")
};

// 对应蓝图：E_ItemRarity (物品星级/稀有度)
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Star1       UMETA(DisplayName = "★"),
    Star2       UMETA(DisplayName = "★★"),
    Star3       UMETA(DisplayName = "★★★"),
    Star4       UMETA(DisplayName = "★★★★"),
    Star5       UMETA(DisplayName = "★★★★★")
};


// ======= USTRUCT (结构体) =======

// 对应蓝图：F_ItemData (物品静态数据表)
// 继承 FTableRowBase 是为了能让这个结构体作为 DataTable (数据表格) 的行数据使用
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    // 1. 物品ID (整数)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemID = 0;

    // 2. 物品名称 (文本)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FText ItemName;

    // 3. 物品详情介绍 (文本)
    // meta = (MultiLine = "true") 可以在UE编辑器里允许多行输入，方便填表
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = "true"))
    FText ItemDescription;

    // 4. 物品图像 (纹理2D)
    // 使用 TSoftObjectPtr (软引用) 而不是 UTexture2D* (硬引用)，防止游戏一启动就把所有图标全加载进内存导致卡顿
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UTexture2D> ItemIcon;

    // 5. 是否可堆叠 (布尔)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    bool bIsStackable = false;

    // 6. 最大可堆叠数量 (可堆叠物品默认9999，不可堆叠物品固定为1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (EditCondition = "bIsStackable", ClampMin = "1"))
    int32 MaxStackSize = 9999;

    // 7. 物品星级 (E_ItemRarity 枚举)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemRarity ItemRarity = EItemRarity::Star1;

    // 8. 物品类型 (E_ItemCategory 枚举)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemCategory ItemCategory = EItemCategory::Weapon;

    // 9. 物品建模 (静态网格体)
    // 同样使用软引用优化内存
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UStaticMesh> ItemMesh;

    // 10. 物品功能介绍 (文本)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = "true"))
    FText ItemFunctionDescription;
};

// ==========================================
// 背包系统专属分类标签页的数据结构体 (用于 UI 读取和生成 Tab)
// ==========================================
USTRUCT(BlueprintType)
struct FInventoryCategoryTabData : public FTableRowBase
{
    GENERATED_BODY()

    // 1. 标签页名称 (对应蓝图的 CategoryNameText)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    FText CategoryNameText;

    // 2. 标签页图标 (对应蓝图的 CategoryIcon)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    TSoftObjectPtr<UTexture2D> CategoryIcon;

    // 3. 关联的物品分类枚举 (对应蓝图的 TabCategory)
    // 这是核心的“钥匙”，UI 点击这个 Tab 时，用这个枚举去过滤背包物品
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Tab Data")
    EItemCategory TabCategory = EItemCategory::Weapon;
};