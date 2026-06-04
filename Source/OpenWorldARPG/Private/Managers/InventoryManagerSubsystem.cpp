// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Managers/InventoryManagerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Managers/GameAssetManagerSubsystem.h"

void UInventoryManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    ItemDatabase = AssetManager ? AssetManager->GetItemDatabaseTable() : nullptr;

    if (!ItemDatabase)
    {
        UE_LOG(LogTemp, Error, TEXT("InventoryManager: 无法加载物品数据表！"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("InventoryManager: 物品数据表加载成功。"));
    }
}

const FItemData* UInventoryManagerSubsystem::GetItemData(int32 ItemID) const
{
    if (!ItemDatabase) return nullptr;

    FName RowName = FName(*FString::FromInt(ItemID));
    return ItemDatabase->FindRow<FItemData>(RowName, TEXT("InventoryManager"));
}

// =====================================================================
// 可堆叠物品添加：同ID合并，受 MaxStackSize 约束，超出拒绝
// =====================================================================

void UInventoryManagerSubsystem::AddStackableItem(int32 ItemID, int32 Amount)
{
    if (Amount <= 0) return;

    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddStackableItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (!ItemConfig->bIsStackable)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddStackableItem: 物品ID %d 不是可堆叠物品，请使用 AddUniqueItem。"), ItemID);
        return;
    }

    const int32 MaxStack = ItemConfig->MaxStackSize > 0 ? ItemConfig->MaxStackSize : 9999;

    // 1. 查找已有格子，填充至 MaxStackSize 上限
    FItemInstance* ExistingSlot = nullptr;
    for (FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemID == ItemID && Item.bIsStackable)
        {
            ExistingSlot = &Item;
            break;
        }
    }

    int32 ActualAdded = 0;

    if (ExistingSlot)
    {
        // 已有格子：填充剩余空间，超出上限的部分放弃
        const int32 SpaceInSlot = MaxStack - ExistingSlot->Count;
        if (SpaceInSlot > 0)
        {
            ActualAdded = FMath::Min(SpaceInSlot, Amount);
            ExistingSlot->Count += ActualAdded;
        }
    }
    else
    {
        // 没有已有格子：新建一格，数量不超过 MaxStackSize
        if (InventoryItems.Num() >= MaxInventorySize)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("背包满了"));
            }
            return;
        }

        ActualAdded = FMath::Min(MaxStack, Amount);
        FItemInstance NewItem;
        NewItem.ItemID = ItemID;
        NewItem.Count = ActualAdded;
        NewItem.bIsStackable = true;
        InventoryItems.Add(NewItem);
    }

    // 2. 触发事件
    if (ActualAdded > 0)
    {
        OnItemAdded.Broadcast(ItemID, ActualAdded);
        OnInventoryUpdated.Broadcast();
    }

    // 3. 超出上限提示
    const int32 Overflow = Amount - ActualAdded;
    if (Overflow > 0)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
                FString::Printf(TEXT("物品数量超过上限(%d)，%d 个未能添加"), MaxStack, Overflow));
        }
    }
}

// =====================================================================
// 不可堆叠物品添加：每件独占一格，携带实例数据
// =====================================================================

void UInventoryManagerSubsystem::AddUniqueItem(int32 ItemID, const FItemInstanceData& InInstanceData)
{
    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddUniqueItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (ItemConfig->bIsStackable)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddUniqueItem: 物品ID %d 是可堆叠物品，请使用 AddStackableItem。"), ItemID);
        return;
    }

    if (InventoryItems.Num() >= MaxInventorySize)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("背包满了"));
        }
        return;
    }

    FItemInstance NewItem;
    NewItem.ItemID = ItemID;
    NewItem.Count = 1;
    NewItem.bIsStackable = false;
    NewItem.InstanceData = InInstanceData;
    InventoryItems.Add(NewItem);

    OnItemAdded.Broadcast(ItemID, 1);
    OnInventoryUpdated.Broadcast();
}

// =====================================================================
// 兼容旧接口：自动判断物品类型选择添加方式
// =====================================================================

void UInventoryManagerSubsystem::AddItem(int32 ItemID, int32 Amount)
{
    if (Amount <= 0) return;

    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (ItemConfig->bIsStackable)
    {
        AddStackableItem(ItemID, Amount);
    }
    else
    {
        // 不可堆叠物品：Amount 件 = Amount 个独立格子
        for (int32 i = 0; i < Amount; ++i)
        {
            FItemInstanceData DefaultData;
            AddUniqueItem(ItemID, DefaultData);
        }
    }
}

// =====================================================================
// 移除物品
// =====================================================================

bool UInventoryManagerSubsystem::RemoveItemByIndex(int32 DropIndex, int32 DropAmount)
{
    if (DropAmount <= 0 || !InventoryItems.IsValidIndex(DropIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItemByIndex: 无效的丢弃数量或索引。"));
        return false;
    }

    FItemInstance& TargetItem = InventoryItems[DropIndex];
    const int32 ItemIDToDrop = TargetItem.ItemID;

    // 不可堆叠物品：整格移除，忽略 DropAmount
    if (!TargetItem.bIsStackable)
    {
        OnItemDropped.Broadcast(ItemIDToDrop, 1);
        InventoryItems.RemoveAt(DropIndex);
        OnInventoryUpdated.Broadcast();
        return true;
    }

    // 可堆叠物品：按数量扣减
    int32 ActualDropped = 0;
    if (TargetItem.Count <= DropAmount)
    {
        ActualDropped = TargetItem.Count;
        OnItemDropped.Broadcast(ItemIDToDrop, ActualDropped);
        InventoryItems.RemoveAt(DropIndex);
    }
    else
    {
        ActualDropped = DropAmount;
        TargetItem.Count -= DropAmount;
        OnItemDropped.Broadcast(ItemIDToDrop, ActualDropped);
    }

    OnInventoryUpdated.Broadcast();
    return true;
}

// =====================================================================
// 查询接口
// =====================================================================

TArray<FItemInstance> UInventoryManagerSubsystem::GetItemsByCategory(EItemCategory Category) const
{
    TArray<FItemInstance> FilteredItems;

    for (const FItemInstance& Item : InventoryItems)
    {
        FItemData StaticData;
        if (GetItemStaticData(Item.ItemID, StaticData) && StaticData.ItemCategory == Category)
        {
            FilteredItems.Add(Item);
        }
    }

    return FilteredItems;
}

int32 UInventoryManagerSubsystem::GetItemCountByID(int32 ItemID) const
{
    int32 TotalCount = 0;
    for (const FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemID == ItemID)
        {
            TotalCount += Item.Count;
        }
    }
    return TotalCount;
}

bool UInventoryManagerSubsystem::GetItemInstanceAtIndex(int32 Index, FItemInstance& OutInstance) const
{
    if (InventoryItems.IsValidIndex(Index))
    {
        OutInstance = InventoryItems[Index];
        return true;
    }
    return false;
}

bool UInventoryManagerSubsystem::GetItemStaticData(int32 ItemID, FItemData& OutItemData) const
{
    if (!ItemDatabase) return false;

    FName RowName = FName(*FString::FromInt(ItemID));
    FItemData* FoundRow = ItemDatabase->FindRow<FItemData>(RowName, TEXT("GetItemStaticData"));

    if (FoundRow)
    {
        OutItemData = *FoundRow;
        return true;
    }
    return false;
}
