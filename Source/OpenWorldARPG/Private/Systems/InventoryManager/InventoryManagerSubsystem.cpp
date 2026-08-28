// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InventoryManager/InventoryManagerSubsystem.h"
#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"

void UInventoryManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UGameAssetManagerSubsystem* AssetManager = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    ItemDatabase = AssetManager ? AssetManager->GetItemDatabaseTable() : nullptr;

    if (!ItemDatabase)
    {
        UE_LOG(LogTemp, Error, TEXT("InventoryManager: 无法加载物品数据表！"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("InventoryManager: 物品数据表加载成功。"));
    }

    CategoryCapacityLimits.Add(EItemCategory::Weapon, 1000);
    CategoryCapacityLimits.Add(EItemCategory::Artifact, 1500);
    CategoryCapacityLimits.Add(EItemCategory::Material, 9999);
    CategoryCapacityLimits.Add(EItemCategory::Food, 2000);
    CategoryCapacityLimits.Add(EItemCategory::Quest, 100);
}

const FItemData* UInventoryManagerSubsystem::GetItemData(int32 ItemID) const
{
    if (!ItemDatabase) return nullptr;
    FName RowName = FName(*FString::FromInt(ItemID));
    return ItemDatabase->FindRow<FItemData>(RowName, TEXT("InventoryManager"));
}

// --- 内部辅助 ---

UGameServerSubsystem* UInventoryManagerSubsystem::GetGameServer() const
{
    return GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameServerSubsystem>();
}

// --- 服务器权威 ---

void UInventoryManagerSubsystem::ApplyServerSnapshot(const TArray<FItemInstance>& Items)
{
    // 索引旧状态用于差分事件
    TMap<FGuid, FItemInstance> OldItems;
    for (const FItemInstance& Item : InventoryItems)
    {
        OldItems.Add(Item.ItemGUID, Item);
    }

    InventoryItems = Items;

    TSet<FGuid> NewGuids;
    NewGuids.Reserve(InventoryItems.Num());
    for (const FItemInstance& NewItem : InventoryItems)
    {
        NewGuids.Add(NewItem.ItemGUID);

        const FItemInstance* Old = OldItems.Find(NewItem.ItemGUID);
        if (!Old)
        {
            OnItemAdded.Broadcast(NewItem.ItemID, NewItem.Count);
        }
    }

    for (const TPair<FGuid, FItemInstance>& Pair : OldItems)
    {
        if (!NewGuids.Contains(Pair.Key))
        {
            OnItemDropped.Broadcast(Pair.Value.ItemID, Pair.Value.Count);
        }
    }

    OnInventoryUpdated.Broadcast();
}

int32 UInventoryManagerSubsystem::FindIndexByGUID(FGuid ItemGUID) const
{
    for (int32 i = 0; i < InventoryItems.Num(); ++i)
    {
        if (InventoryItems[i].ItemGUID == ItemGUID) return i;
    }
    return INDEX_NONE;
}

bool UInventoryManagerSubsystem::HasCategorySpace(EItemCategory Category) const
{
    const int32* Capacity = CategoryCapacityLimits.Find(Category);
    if (!Capacity) return true;
    return GetCategoryItemCount(Category) < *Capacity;
}

int32 UInventoryManagerSubsystem::GetCategoryItemCount(EItemCategory Category) const
{
    int32 Count = 0;
    for (const FItemInstance& Item : InventoryItems)
    {
        const FItemData* StaticData = GetItemData(Item.ItemID);
        if (StaticData && StaticData->ItemCategory == Category) Count++;
    }
    return Count;
}

int32 UInventoryManagerSubsystem::GetCategoryCapacity(EItemCategory Category) const
{
    const int32* Capacity = CategoryCapacityLimits.Find(Category);
    return Capacity ? *Capacity : 9999;
}

// --- 添加物品 ---

void UInventoryManagerSubsystem::AddStackableItem(int32 ItemID, int32 Amount)
{
    if (Amount <= 0) return;

    if (UGameServerSubsystem* Server = GetGameServer())
    {
        if (Server->IsLoggedIn())
        {
            Server->ServerAddItem(ItemID, Amount);
            return;
        }
    }

    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddStackableItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (!ItemConfig->bIsStackable)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddStackableItem: 物品ID %d 不是可堆叠物品。"), ItemID);
        return;
    }

    if (!HasCategorySpace(ItemConfig->ItemCategory))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("该分类背包已满"));
        return;
    }

    const int32 MaxStack = ItemConfig->MaxStackSize > 0 ? ItemConfig->MaxStackSize : 9999;

    FItemInstance* ExistingSlot = nullptr;
    for (FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemID == ItemID) { ExistingSlot = &Item; break; }
    }

    int32 ActualAdded = 0;

    if (ExistingSlot)
    {
        const int32 SpaceInSlot = MaxStack - ExistingSlot->Count;
        if (SpaceInSlot > 0)
        {
            ActualAdded = FMath::Min(SpaceInSlot, Amount);
            ExistingSlot->Count += ActualAdded;
        }
    }
    else
    {
        ActualAdded = FMath::Min(MaxStack, Amount);
        FItemInstance NewItem;
        NewItem.ItemGUID = FGuid::NewGuid();
        NewItem.ItemID = ItemID;
        NewItem.Count = ActualAdded;
        NewItem.AcquiredTime = FDateTime::UtcNow();
        InventoryItems.Add(NewItem);
    }

    if (ActualAdded > 0)
    {
        OnItemAdded.Broadcast(ItemID, ActualAdded);
        OnInventoryUpdated.Broadcast();
    }

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

void UInventoryManagerSubsystem::AddItem(int32 ItemID, int32 Amount)
{
    if (Amount <= 0) return;

    if (UGameServerSubsystem* Server = GetGameServer())
    {
        if (Server->IsLoggedIn())
        {
            // 可堆叠与非堆叠（武器/圣遗物数量）统一由服务器处理
            Server->ServerAddItem(ItemID, Amount);
            return;
        }
    }

    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (ItemConfig->bIsStackable)
    {
        AddStackableItem(ItemID, Amount);
        return;
    }

    // 不可堆叠: 每个实例独立槽位 (count = 1)
    if (!HasCategorySpace(ItemConfig->ItemCategory))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("该分类背包已满"));
        return;
    }

    for (int32 i = 0; i < Amount; ++i)
    {
        FItemInstance NewItem;
        NewItem.ItemGUID = FGuid::NewGuid();
        NewItem.ItemID = ItemID;
        NewItem.Count = 1;
        NewItem.AcquiredTime = FDateTime::UtcNow();
        InventoryItems.Add(NewItem);
    }

    OnItemAdded.Broadcast(ItemID, Amount);
    OnInventoryUpdated.Broadcast();
}

// --- 移除物品 ---

bool UInventoryManagerSubsystem::RemoveItemByGUID(FGuid ItemGUID, int32 RemoveAmount)
{
    if (UGameServerSubsystem* Server = GetGameServer())
    {
        if (Server->IsLoggedIn())
        {
            Server->ServerRemoveItem(ItemGUID, RemoveAmount);
            return true;
        }
    }

    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItemByGUID: GUID %s 未找到。"), *ItemGUID.ToString());
        return false;
    }
    return RemoveItemByIndex(Index, RemoveAmount);
}

bool UInventoryManagerSubsystem::RemoveItemByIndex(int32 DropIndex, int32 DropAmount)
{
    if (DropAmount <= 0 || !InventoryItems.IsValidIndex(DropIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItemByIndex: 无效的丢弃数量或索引。"));
        return false;
    }

    if (UGameServerSubsystem* Server = GetGameServer())
    {
        if (Server->IsLoggedIn())
        {
            Server->ServerRemoveItem(InventoryItems[DropIndex].ItemGUID, DropAmount);
            return true;
        }
    }

    FItemInstance& TargetItem = InventoryItems[DropIndex];
    const int32 ItemIDToDrop = TargetItem.ItemID;

    const FItemData* ItemConfig = GetItemData(ItemIDToDrop);
    const bool bIsStackable = ItemConfig ? ItemConfig->bIsStackable : false;

    if (!bIsStackable)
    {
        OnItemDropped.Broadcast(ItemIDToDrop, 1);
        InventoryItems.RemoveAt(DropIndex);
        OnInventoryUpdated.Broadcast();
        return true;
    }

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

// --- 查询 ---

void UInventoryManagerSubsystem::GetItemsByCategory(EItemCategory Category, TArray<FItemInstance>& OutItems) const
{
    OutItems.Empty();
    for (const FItemInstance& Item : InventoryItems)
    {
        const FItemData* StaticData = GetItemData(Item.ItemID);
        if (StaticData && StaticData->ItemCategory == Category) OutItems.Add(Item);
    }
}

void UInventoryManagerSubsystem::GetItemsByFilter(EItemCategory Category, EItemRarity RarityFilter, TArray<FItemInstance>& OutItems) const
{
    OutItems.Empty();

    for (const FItemInstance& Item : InventoryItems)
    {
        const FItemData* StaticData = GetItemData(Item.ItemID);
        if (!StaticData || StaticData->ItemCategory != Category) continue;

        if (RarityFilter != EItemRarity::Star1)
        {
            if (StaticData->ItemRarity != RarityFilter) continue;
        }

        OutItems.Add(Item);
    }
}

int32 UInventoryManagerSubsystem::GetItemCountByID(int32 ItemID) const
{
    int32 TotalCount = 0;
    for (const FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemID == ItemID) TotalCount += Item.Count;
    }
    return TotalCount;
}

bool UInventoryManagerSubsystem::GetItemInstanceByGUID(FGuid ItemGUID, FItemInstance& OutInstance) const
{
    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index != INDEX_NONE)
    {
        OutInstance = InventoryItems[Index];
        return true;
    }
    return false;
}

bool UInventoryManagerSubsystem::GetItemStaticData(int32 ItemID, FItemData& OutItemData) const
{
    const FItemData* FoundRow = GetItemData(ItemID);
    if (FoundRow)
    {
        OutItemData = *FoundRow;
        return true;
    }
    return false;
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
