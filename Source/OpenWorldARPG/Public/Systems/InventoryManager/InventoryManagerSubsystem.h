// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "Systems/InventoryManager/Types/ArtifactTypes.h"
#include "Systems/InventoryManager/Types/WeaponTypes.h"
#include "Systems/InventoryManager/Data/ItemData.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "InventoryManagerSubsystem.generated.h"

class UGameServerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, int32, ItemID, int32, AddedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, int32, ItemID, int32, DroppedAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemEquipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUnequipped, FGuid, ItemGUID, int32, ItemID, int32, CharacterID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, FGuid, ItemGUID, int32, ItemID, int32, UsedAmount);

/**
 * 背包管理子系统 (Model 层)
 * GUID 驱动、分类容量限制、装备与消耗。纯业务数据，不含 UI 逻辑。
 *
 * 服务器权威模式 (M3)：已登录 GameServer 时，所有变更接口只向服务器转发请求，
 * 本地状态唯一来源是 ApplyServerSnapshot 应用服务器快照；
 * 未登录（编辑器离线调试）时回退为本地直改。
 */
UCLASS()
class OPENWORLDARPG_API UInventoryManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 服务器权威 ---

    // 应用服务器下发的背包全量快照（登录响应 / 操作确认），并差分广播事件
    void ApplyServerSnapshot(const TArray<FItemInstance>& Items,
                             const TMap<FGuid, FWeaponInstanceData>& WeaponMap,
                             const TMap<FGuid, FArtifactInstanceData>& ArtifactMap);

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
    UGameServerSubsystem* GetGameServer() const;

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
