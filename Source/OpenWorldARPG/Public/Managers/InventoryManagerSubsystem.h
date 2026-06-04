// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/SharedTypes.h"
#include "InventoryManagerSubsystem.generated.h"

// --- 委托 ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, int32, ItemID, int32, AddedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, int32, ItemID, int32, DroppedAmount);

/**
 * @struct FItemInstanceData
 * @brief 不可堆叠物品的实例数据（武器、圣遗物等），每个实例独占一格。
 */
USTRUCT(BlueprintType)
struct FItemInstanceData
{
    GENERATED_BODY()

    /** 武器等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    int32 Level = 1;

    /** 突破等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    int32 AscensionLevel = 0;

    /** 精炼等级 (仅武器) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    int32 RefinementLevel = 1;

    /** 圣遗物主词条类型 (仅圣遗物) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    FName MainStatType;

    /** 圣遗物主词条数值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    float MainStatValue = 0.f;

    /** 圣遗物副词条 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Instance")
    TMap<FName, float> SubStats;
};

/**
 * @struct FItemInstance
 * @brief 背包中的物品实例。
 * 可堆叠物品：ItemID + Count，同 ID 共用一格，Count <= MaxStackSize
 * 不可堆叠物品：ItemID + Count=1 + InstanceData，每件独占一格
 */
USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY()

    /** 物品静态ID (对应 DataTable 行) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ItemID = 0;

    /** 数量 (可堆叠物品: 1~MaxStackSize; 不可堆叠物品: 固定为1) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Count = 0;

    /** 不可堆叠物品的实例数据 (可堆叠物品不使用) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FItemInstanceData InstanceData;

    /** 是否为可堆叠物品 (从 FItemData 缓存，避免频繁查表) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bIsStackable = false;
};

/**
 * @class UInventoryManagerSubsystem
 * @brief 背包管理子系统。负责物品的增删查改与堆叠逻辑。
 */
UCLASS()
class OPENWORLDARPG_API UInventoryManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 核心 API ---

    /** 添加可堆叠物品，同 ID 合并，受 MaxStackSize 约束 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddStackableItem(int32 ItemID, int32 Amount);

    /** 添加不可堆叠物品，每件独占一格，携带实例数据 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddUniqueItem(int32 ItemID, const FItemInstanceData& InInstanceData);

    /** 自动判断物品类型选择添加方式 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddItem(int32 ItemID, int32 Amount);

    /** 按索引移除物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemByIndex(int32 DropIndex, int32 DropAmount = 1);

    // --- 事件 ---
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryUpdated OnInventoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemAdded OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemDropped OnItemDropped;

    // --- 查询 ---

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    const TArray<FItemInstance>& GetAllInventoryItems() const { return InventoryItems; }

    /** 按物品分类筛选 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    TArray<FItemInstance> GetItemsByCategory(EItemCategory Category) const;

    /** 查询可堆叠物品的总数量 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetItemCountByID(int32 ItemID) const;

    /** 获取指定索引的物品实例 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemInstanceAtIndex(int32 Index, FItemInstance& OutInstance) const;

    /** 获取物品的静态配置数据 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemStaticData(int32 ItemID, FItemData& OutItemData) const;

private:
    const FItemData* GetItemData(int32 ItemID) const;

    const int32 MaxInventorySize = 2000;

    UPROPERTY()
    TArray<FItemInstance> InventoryItems;

    UPROPERTY()
    UDataTable* ItemDatabase;
};
