// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/ItemTypes.h"
#include "Types/ArtifactTypes.h"
#include "Types/WeaponTypes.h"
#include "Types/ItemData.h"
#include "Types/ItemInstance.h"
#include "InventoryManagerSubsystem.generated.h"

// --- 委托 ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, int32, ItemID, int32, AddedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, int32, ItemID, int32, DroppedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemEquipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUnequipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, FGuid, ItemGUID, int32, ItemID, int32, UsedAmount);

/**
 * @class UInventoryManagerSubsystem
 * @brief 背包管理子系统。对标《鸣潮》标准：GUID 驱动、分类容量、排序筛选、装备/使用。
 */
UCLASS()
class OPENWORLDARPG_API UInventoryManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // ==========================================
    // 添加物品
    // ==========================================

    /** 添加可堆叠物品，同 ID 合并，受 MaxStackSize 约束 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddStackableItem(int32 ItemID, int32 Amount);

    /** 添加不可堆叠物品 (武器/圣遗物)，每件独占一格 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddUniqueItem(int32 ItemID, const FWeaponInstanceData& WeaponData);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddArtifactItem(int32 ItemID, const FArtifactInstanceData& ArtifactData);

    /** 自动判断物品类型选择添加方式 (不可堆叠物品使用默认实例数据) */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddItem(int32 ItemID, int32 Amount);

    // ==========================================
    // 移除物品 (GUID 驱动)
    // ==========================================

    /** 按 GUID 移除物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemByGUID(FGuid ItemGUID, int32 RemoveAmount = 1);

    /** 按索引移除物品 (保留兼容) */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemByIndex(int32 DropIndex, int32 DropAmount = 1);

    // ==========================================
    // 装备系统
    // ==========================================

    /** 装备物品到角色 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool EquipItem(FGuid ItemGUID, int32 CharacterID);

    /** 卸下物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool UnequipItem(FGuid ItemGUID);

    /** 卸下角色所有装备 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void UnequipAllForCharacter(int32 CharacterID);

    /** 查询角色装备的武器 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    FItemInstance GetEquippedWeapon(int32 CharacterID) const;

    /** 查询角色装备的圣遗物 (按部位) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    TArray<FItemInstance> GetEquippedArtifacts(int32 CharacterID) const;

    // ==========================================
    // 使用/消耗系统
    // ==========================================

    /** 使用物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Use")
    bool UseItem(FGuid ItemGUID, int32 TargetCharacterID = -1, int32 UseAmount = 1);

    // ==========================================
    // 事件
    // ==========================================

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryUpdated OnInventoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemAdded OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemDropped OnItemDropped;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemEquipped OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemUnequipped OnItemUnequipped;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemUsed OnItemUsed;

    // ==========================================
    // 查询
    // ==========================================

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    const TArray<FItemInstance>& GetAllInventoryItems() const { return InventoryItems; }

    /** 按物品分类筛选 (返回引用，避免拷贝) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    void GetItemsByCategory(EItemCategory Category, TArray<FItemInstance>& OutItems) const;

    /** 按分类 + 排序模式 + 稀有度筛选 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    void GetItemsByFilter(EItemCategory Category, EItemSortMode SortMode, EItemRarity RarityFilter, TArray<FItemInstance>& OutItems) const;

    /** 查询可堆叠物品的总数量 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetItemCountByID(int32 ItemID) const;

    /** 通过 GUID 查找物品实例 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemInstanceByGUID(FGuid ItemGUID, FItemInstance& OutInstance) const;

    /** 获取指定索引的物品实例 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemInstanceAtIndex(int32 Index, FItemInstance& OutInstance) const;

    /** 获取物品的静态配置数据 (值拷贝) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemStaticData(int32 ItemID, FItemData& OutItemData) const;

    /** 获取物品的静态配置数据指针 (只读) */
    const FItemData* GetItemData(int32 ItemID) const;

    /** 查询分类当前物品数量 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetCategoryItemCount(EItemCategory Category) const;

    /** 查询分类容量上限 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetCategoryCapacity(EItemCategory Category) const;

private:
    /** 内部：按 GUID 查找数组索引 */
    int32 FindIndexByGUID(FGuid ItemGUID) const;

    /** 内部：排序实现 */
    void SortItems(TArray<FItemInstance>& Items, EItemSortMode SortMode) const;

    /** 内部：检查分类容量 */
    bool HasCategorySpace(EItemCategory Category) const;

    UPROPERTY()
    TArray<FItemInstance> InventoryItems;

    UPROPERTY()
    UDataTable* ItemDatabase;

    // --- 分类容量配置 ---
    TMap<EItemCategory, int32> CategoryCapacityLimits;
};
