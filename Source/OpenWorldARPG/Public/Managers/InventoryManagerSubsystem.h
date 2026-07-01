// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/ItemTypes.h"
#include "Types/ArtifactTypes.h"
#include "Types/WeaponTypes.h"
#include "Types/ItemData.h"
#include "Types/ItemInstance.h"
#include "InventoryManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, int32, ItemID, int32, AddedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, int32, ItemID, int32, DroppedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemEquipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUnequipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, FGuid, ItemGUID, int32, ItemID, int32, UsedAmount);

/**
 * 背包管理子系统 (Model 层)
 * GUID 驱动、分类容量限制、装备与消耗。纯业务数据，不含 UI 逻辑。
 */
UCLASS()
class OPENWORLDARPG_API UInventoryManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 添加物品 ---

    void AddStackableItem(int32 ItemID, int32 Amount);
    void AddUniqueItem(int32 ItemID, const FWeaponInstanceData& WeaponData);
    void AddArtifactItem(int32 ItemID, const FArtifactInstanceData& ArtifactData);
    void AddItem(int32 ItemID, int32 Amount);

    // --- 移除物品 ---

    bool RemoveItemByGUID(FGuid ItemGUID, int32 RemoveAmount = 1);
    bool RemoveItemByIndex(int32 DropIndex, int32 DropAmount = 1);
    void DiscardItemByGUID(FGuid ItemGUID, int32 Amount = 1) { RemoveItemByGUID(ItemGUID, Amount); }

    // --- 装备系统 ---

    bool EquipItem(FGuid ItemGUID, int32 CharacterID);
    bool UnequipItem(FGuid ItemGUID);
    void UnequipAllForCharacter(int32 CharacterID);
    FItemInstance GetEquippedWeapon(int32 CharacterID) const;
    TArray<FItemInstance> GetEquippedArtifacts(int32 CharacterID) const;

    // --- 使用/消耗 ---

    bool UseItem(FGuid ItemGUID, int32 TargetCharacterID = -1, int32 UseAmount = 1);

    // --- 查询 ---

    const TArray<FItemInstance>& GetAllInventoryItems() const { return InventoryItems; }
    void GetItemsByCategory(EItemCategory Category, TArray<FItemInstance>& OutItems) const;
    void GetItemsByFilter(EItemCategory Category, EItemRarity RarityFilter, TArray<FItemInstance>& OutItems) const;
    int32 GetItemCountByID(int32 ItemID) const;
    bool GetItemInstanceByGUID(FGuid ItemGUID, FItemInstance& OutInstance) const;
    bool GetItemInstanceAtIndex(int32 Index, FItemInstance& OutInstance) const;
    bool GetItemStaticData(int32 ItemID, FItemData& OutItemData) const;
    const FItemData* GetItemData(int32 ItemID) const;

    // --- 实例数据查询 ---

    bool GetWeaponInstanceData(FGuid ItemGUID, FWeaponInstanceData& OutData) const;
    bool GetArtifactInstanceData(FGuid ItemGUID, FArtifactInstanceData& OutData) const;
    int32 GetCategoryItemCount(EItemCategory Category) const;
    int32 GetCategoryCapacity(EItemCategory Category) const;

private:
    // --- 内部辅助 ---

    int32 FindIndexByGUID(FGuid ItemGUID) const;
    bool HasCategorySpace(EItemCategory Category) const;

public:
    // --- 事件委托 ---

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

private:
    // --- 数据存储 ---

    UPROPERTY()
    TArray<FItemInstance> InventoryItems;

    // --- 类型实例映射 ---

    UPROPERTY()
    TMap<FGuid, FWeaponInstanceData> WeaponInstanceMap;

    UPROPERTY()
    TMap<FGuid, FArtifactInstanceData> ArtifactInstanceMap;

    UPROPERTY()
    UDataTable* ItemDatabase;

    // --- 容量配置 ---

    TMap<EItemCategory, int32> CategoryCapacityLimits;
};
