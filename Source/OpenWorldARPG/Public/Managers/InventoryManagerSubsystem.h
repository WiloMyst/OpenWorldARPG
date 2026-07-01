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

// --- 委托 ---
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

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddUniqueItem(int32 ItemID, const FWeaponInstanceData& WeaponData);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddArtifactItem(int32 ItemID, const FArtifactInstanceData& ArtifactData);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddItem(int32 ItemID, int32 Amount);

    // --- 移除物品 (GUID 驱动) ---

    bool RemoveItemByGUID(FGuid ItemGUID, int32 RemoveAmount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemByIndex(int32 DropIndex, int32 DropAmount = 1);

    // MVVM：供 ViewModel 调用的丢弃接口
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void DiscardItemByGUID(FGuid ItemGUID, int32 Amount = 1) { RemoveItemByGUID(ItemGUID, Amount); }

    // --- 装备系统 ---

    bool EquipItem(FGuid ItemGUID, int32 CharacterID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool UnequipItem(FGuid ItemGUID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void UnequipAllForCharacter(int32 CharacterID);

    FItemInstance GetEquippedWeapon(int32 CharacterID) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    TArray<FItemInstance> GetEquippedArtifacts(int32 CharacterID) const;

    // --- 使用/消耗系统 ---

    UFUNCTION(BlueprintCallable, Category = "Inventory|Use")
    bool UseItem(FGuid ItemGUID, int32 TargetCharacterID = -1, int32 UseAmount = 1);

    // --- 事件 ---

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

    // --- 查询 ---

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    const TArray<FItemInstance>& GetAllInventoryItems() const { return InventoryItems; }

    void GetItemsByCategory(EItemCategory Category, TArray<FItemInstance>& OutItems) const;

    /** 获取过滤后的数据（排序交由 ViewModel 处理） */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    void GetItemsByFilter(EItemCategory Category, EItemRarity RarityFilter, TArray<FItemInstance>& OutItems) const;

    int32 GetItemCountByID(int32 ItemID) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemInstanceByGUID(FGuid ItemGUID, FItemInstance& OutInstance) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemInstanceAtIndex(int32 Index, FItemInstance& OutInstance) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    bool GetItemStaticData(int32 ItemID, FItemData& OutItemData) const;

    const FItemData* GetItemData(int32 ItemID) const;

    // --- 类型专属实例数据（外键存储，避免内存膨胀） ---

    UFUNCTION(BlueprintPure, Category = "Inventory|Weapon")
    bool GetWeaponInstanceData(FGuid ItemGUID, FWeaponInstanceData& OutData) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Artifact")
    bool GetArtifactInstanceData(FGuid ItemGUID, FArtifactInstanceData& OutData) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetCategoryItemCount(EItemCategory Category) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Query")
    int32 GetCategoryCapacity(EItemCategory Category) const;

private:
    int32 FindIndexByGUID(FGuid ItemGUID) const;
    bool HasCategorySpace(EItemCategory Category) const;

    UPROPERTY()
    TArray<FItemInstance> InventoryItems;

    // --- 类型专属实例数据（外键存储，避免内存膨胀） ---

    UPROPERTY()
    TMap<FGuid, FWeaponInstanceData> WeaponInstanceMap;

    UPROPERTY()
    TMap<FGuid, FArtifactInstanceData> ArtifactInstanceMap;

    UPROPERTY()
    UDataTable* ItemDatabase;

    // --- 分类容量配置 ---
    TMap<EItemCategory, int32> CategoryCapacityLimits;
};