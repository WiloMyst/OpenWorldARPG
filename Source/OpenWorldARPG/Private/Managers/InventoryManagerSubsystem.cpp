// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/InventoryManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

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

void UInventoryManagerSubsystem::AddUniqueItem(int32 ItemID, const FWeaponInstanceData& WeaponData)
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

    if (!HasCategorySpace(ItemConfig->ItemCategory))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("该分类背包已满"));
        return;
    }

    FItemInstance NewItem;
    NewItem.ItemGUID = FGuid::NewGuid();
    NewItem.ItemID = ItemID;
    NewItem.Count = 1;
    NewItem.AcquiredTime = FDateTime::UtcNow();
    InventoryItems.Add(NewItem);

    WeaponInstanceMap.Add(NewItem.ItemGUID, WeaponData);

    OnItemAdded.Broadcast(ItemID, 1);
    OnInventoryUpdated.Broadcast();
}

void UInventoryManagerSubsystem::AddArtifactItem(int32 ItemID, const FArtifactInstanceData& ArtifactData)
{
    const FItemData* ItemConfig = GetItemData(ItemID);
    if (!ItemConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddArtifactItem: 物品ID %d 不存在于数据表中！"), ItemID);
        return;
    }

    if (!HasCategorySpace(ItemConfig->ItemCategory))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("圣遗物背包已满"));
        return;
    }

    FItemInstance NewItem;
    NewItem.ItemGUID = FGuid::NewGuid();
    NewItem.ItemID = ItemID;
    NewItem.Count = 1;
    NewItem.AcquiredTime = FDateTime::UtcNow();
    InventoryItems.Add(NewItem);

    ArtifactInstanceMap.Add(NewItem.ItemGUID, ArtifactData);

    OnItemAdded.Broadcast(ItemID, 1);
    OnInventoryUpdated.Broadcast();
}

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
        if (ItemConfig->ItemCategory == EItemCategory::Weapon)
        {
            for (int32 i = 0; i < Amount; ++i)
            {
                FWeaponInstanceData DefaultWeaponData;
                AddUniqueItem(ItemID, DefaultWeaponData);
            }
        }
        else if (ItemConfig->ItemCategory == EItemCategory::Artifact)
        {
            for (int32 i = 0; i < Amount; ++i)
            {
                FArtifactInstanceData DefaultArtifactData;
                AddArtifactItem(ItemID, DefaultArtifactData);
            }
        }
        else
        {
            for (int32 i = 0; i < Amount; ++i)
            {
                FWeaponInstanceData EmptyData;
                AddUniqueItem(ItemID, EmptyData);
            }
        }
    }
}

// --- 移除物品 ---

bool UInventoryManagerSubsystem::RemoveItemByGUID(FGuid ItemGUID, int32 RemoveAmount)
{
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

    FItemInstance& TargetItem = InventoryItems[DropIndex];
    const int32 ItemIDToDrop = TargetItem.ItemID;
    const FGuid ItemGUIDToDrop = TargetItem.ItemGUID;

    if (TargetItem.EquippedCharacterID >= 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("装备中的物品无法丢弃，请先卸下"));
        return false;
    }

    const FItemData* ItemConfig = GetItemData(ItemIDToDrop);
    const bool bIsStackable = ItemConfig ? ItemConfig->bIsStackable : false;

    if (!bIsStackable)
    {
        WeaponInstanceMap.Remove(ItemGUIDToDrop);
        ArtifactInstanceMap.Remove(ItemGUIDToDrop);

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

// --- 装备系统 ---

bool UInventoryManagerSubsystem::EquipItem(FGuid ItemGUID, int32 CharacterID)
{
    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index == INDEX_NONE) return false;

    FItemInstance& Item = InventoryItems[Index];
    if (Item.EquippedCharacterID >= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipItem: 物品已装备在角色 %d 上。"), Item.EquippedCharacterID);
        return false;
    }

    const FItemData* ItemConfig = GetItemData(Item.ItemID);
    if (!ItemConfig) return false;
    const EItemCategory ItemCategory = ItemConfig->ItemCategory;

    if (ItemCategory == EItemCategory::Weapon)
    {
        FItemInstance OldWeapon = GetEquippedWeapon(CharacterID);
        if (OldWeapon.ItemGUID.IsValid())
        {
            UnequipItem(OldWeapon.ItemGUID);
        }
    }

    if (ItemCategory == EItemCategory::Artifact)
    {
        const FArtifactInstanceData* TargetArtifactData = ArtifactInstanceMap.Find(Item.ItemGUID);
        if (!TargetArtifactData) return false;

        TArray<FItemInstance> CurrentArtifacts = GetEquippedArtifacts(CharacterID);
        for (const FItemInstance& Artifact : CurrentArtifacts)
        {
            const FArtifactInstanceData* EquippedArtifactData = ArtifactInstanceMap.Find(Artifact.ItemGUID);
            if (EquippedArtifactData && EquippedArtifactData->Slot == TargetArtifactData->Slot)
            {
                UnequipItem(Artifact.ItemGUID);
                break;
            }
        }
    }

    Item.EquippedCharacterID = CharacterID;
    OnItemEquipped.Broadcast(ItemGUID, Item.ItemID, CharacterID);
    OnInventoryUpdated.Broadcast();
    return true;
}

bool UInventoryManagerSubsystem::UnequipItem(FGuid ItemGUID)
{
    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index == INDEX_NONE) return false;

    FItemInstance& Item = InventoryItems[Index];
    if (Item.EquippedCharacterID < 0) return false;

    int32 OldCharacterID = Item.EquippedCharacterID;
    Item.EquippedCharacterID = -1;

    OnItemUnequipped.Broadcast(ItemGUID, Item.ItemID, OldCharacterID);
    OnInventoryUpdated.Broadcast();
    return true;
}

void UInventoryManagerSubsystem::UnequipAllForCharacter(int32 CharacterID)
{
    for (FItemInstance& Item : InventoryItems)
    {
        if (Item.EquippedCharacterID == CharacterID) Item.EquippedCharacterID = -1;
    }
    OnInventoryUpdated.Broadcast();
}

FItemInstance UInventoryManagerSubsystem::GetEquippedWeapon(int32 CharacterID) const
{
    for (const FItemInstance& Item : InventoryItems)
    {
        const FItemData* StaticData = GetItemData(Item.ItemID);
        if (StaticData && StaticData->ItemCategory == EItemCategory::Weapon && Item.EquippedCharacterID == CharacterID)
        {
            return Item;
        }
    }
    return FItemInstance();
}

TArray<FItemInstance> UInventoryManagerSubsystem::GetEquippedArtifacts(int32 CharacterID) const
{
    TArray<FItemInstance> Result;
    for (const FItemInstance& Item : InventoryItems)
    {
        const FItemData* StaticData = GetItemData(Item.ItemID);
        if (StaticData && StaticData->ItemCategory == EItemCategory::Artifact && Item.EquippedCharacterID == CharacterID)
        {
            Result.Add(Item);
        }
    }
    return Result;
}

// --- 使用/消耗 ---

bool UInventoryManagerSubsystem::UseItem(FGuid ItemGUID, int32 TargetCharacterID, int32 UseAmount)
{
    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index == INDEX_NONE) return false;

    FItemInstance& Item = InventoryItems[Index];
    const FItemData* ItemConfig = GetItemData(Item.ItemID);
    if (!ItemConfig) return false;

    if (ItemConfig->UseTargetType == EItemUseTarget::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品ID %d 不可使用。"), Item.ItemID);
        return false;
    }

    if (ItemConfig->UseTargetType == EItemUseTarget::SelectCharacter && TargetCharacterID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品ID %d 需要指定目标角色。"), Item.ItemID);
        return false;
    }

    if (ItemConfig->bIsStackable && Item.Count < UseAmount)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品数量不足。"));
        return false;
    }

    if (!ItemConfig->UseEffectClass.IsNull())
    {
        // TODO: 由调用方通过 TargetCharacterID 获取 ASC 应用 GE
        UE_LOG(LogTemp, Log, TEXT("UseItem: 物品ID %d 使用效果需要由调用方应用。"), Item.ItemID);
    }

    if (ItemConfig->bIsStackable)
    {
        Item.Count -= UseAmount;
        if (Item.Count <= 0) InventoryItems.RemoveAt(Index);
    }
    else
    {
        WeaponInstanceMap.Remove(Item.ItemGUID);
        ArtifactInstanceMap.Remove(Item.ItemGUID);
        InventoryItems.RemoveAt(Index);
    }

    OnItemUsed.Broadcast(ItemGUID, Item.ItemID, UseAmount);
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

bool UInventoryManagerSubsystem::GetWeaponInstanceData(FGuid ItemGUID, FWeaponInstanceData& OutData) const
{
    const FWeaponInstanceData* Found = WeaponInstanceMap.Find(ItemGUID);
    if (Found)
    {
        OutData = *Found;
        return true;
    }
    return false;
}

bool UInventoryManagerSubsystem::GetArtifactInstanceData(FGuid ItemGUID, FArtifactInstanceData& OutData) const
{
    const FArtifactInstanceData* Found = ArtifactInstanceMap.Find(ItemGUID);
    if (Found)
    {
        OutData = *Found;
        return true;
    }
    return false;
}
