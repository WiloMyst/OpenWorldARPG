// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/InventoryManagerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

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

    // 初始化分类容量上限
    CategoryCapacityLimits.Add(EItemCategory::Weapon,   1000);
    CategoryCapacityLimits.Add(EItemCategory::Artifact, 1500);
    CategoryCapacityLimits.Add(EItemCategory::Material, 9999);
    CategoryCapacityLimits.Add(EItemCategory::Food,     2000);
    CategoryCapacityLimits.Add(EItemCategory::Quest,    100);
}

const FItemData* UInventoryManagerSubsystem::GetItemData(int32 ItemID) const
{
    if (!ItemDatabase) return nullptr;

    FName RowName = FName(*FString::FromInt(ItemID));
    return ItemDatabase->FindRow<FItemData>(RowName, TEXT("InventoryManager"));
}

// =====================================================================
// 内部辅助
// =====================================================================

int32 UInventoryManagerSubsystem::FindIndexByGUID(FGuid ItemGUID) const
{
    for (int32 i = 0; i < InventoryItems.Num(); ++i)
    {
        if (InventoryItems[i].ItemGUID == ItemGUID)
        {
            return i;
        }
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
        if (Item.ItemCategory == Category)
        {
            Count++;
        }
    }
    return Count;
}

int32 UInventoryManagerSubsystem::GetCategoryCapacity(EItemCategory Category) const
{
    const int32* Capacity = CategoryCapacityLimits.Find(Category);
    return Capacity ? *Capacity : 9999;
}

// =====================================================================
// 排序
// =====================================================================

void UInventoryManagerSubsystem::SortItems(TArray<FItemInstance>& Items, EItemSortMode SortMode) const
{
    switch (SortMode)
    {
    case EItemSortMode::ByRarity:
        Items.Sort([this](const FItemInstance& A, const FItemInstance& B)
        {
            const FItemData* DataA = GetItemData(A.ItemID);
            const FItemData* DataB = GetItemData(B.ItemID);
            if (!DataA || !DataB) return false;
            return static_cast<uint8>(DataA->ItemRarity) > static_cast<uint8>(DataB->ItemRarity);
        });
        break;

    case EItemSortMode::ByLevel:
        Items.Sort([](const FItemInstance& A, const FItemInstance& B)
        {
            // 武器按等级，圣遗物按主词条数值，其他按获取时间
            if (A.ItemCategory == EItemCategory::Weapon && B.ItemCategory == EItemCategory::Weapon)
            {
                return A.WeaponData.Level > B.WeaponData.Level;
            }
            if (A.ItemCategory == EItemCategory::Artifact && B.ItemCategory == EItemCategory::Artifact)
            {
                return A.ArtifactData.MainStatValue > B.ArtifactData.MainStatValue;
            }
            return A.AcquiredTime > B.AcquiredTime;
        });
        break;

    case EItemSortMode::ByTime:
        Items.Sort([](const FItemInstance& A, const FItemInstance& B)
        {
            return A.AcquiredTime > B.AcquiredTime;
        });
        break;

    case EItemSortMode::ByName:
        Items.Sort([this](const FItemInstance& A, const FItemInstance& B)
        {
            const FItemData* DataA = GetItemData(A.ItemID);
            const FItemData* DataB = GetItemData(B.ItemID);
            if (!DataA || !DataB) return false;
            return DataA->ItemName.ToString() < DataB->ItemName.ToString();
        });
        break;
    }
}

// =====================================================================
// 添加物品
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

    if (!HasCategorySpace(ItemConfig->ItemCategory))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("该分类背包已满"));
        return;
    }

    const int32 MaxStack = ItemConfig->MaxStackSize > 0 ? ItemConfig->MaxStackSize : 9999;

    // 查找已有格子
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
        NewItem.ItemCategory = ItemConfig->ItemCategory;
        NewItem.Count = ActualAdded;
        NewItem.bIsStackable = true;
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
    NewItem.ItemCategory = ItemConfig->ItemCategory;
    NewItem.Count = 1;
    NewItem.bIsStackable = false;
    NewItem.WeaponData = WeaponData;
    NewItem.AcquiredTime = FDateTime::UtcNow();
    InventoryItems.Add(NewItem);

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
    NewItem.ItemCategory = EItemCategory::Artifact;
    NewItem.Count = 1;
    NewItem.bIsStackable = false;
    NewItem.ArtifactData = ArtifactData;
    NewItem.AcquiredTime = FDateTime::UtcNow();
    InventoryItems.Add(NewItem);

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
        // 不可堆叠物品：按分类使用默认实例数据
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
            // 其他不可堆叠 (任务物品等)
            for (int32 i = 0; i < Amount; ++i)
            {
                FWeaponInstanceData EmptyData;
                AddUniqueItem(ItemID, EmptyData);
            }
        }
    }
}

// =====================================================================
// 移除物品
// =====================================================================

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

    // 装备中的物品不允许丢弃
    if (TargetItem.EquippedCharacterID >= 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("装备中的物品无法丢弃，请先卸下"));
        return false;
    }

    if (!TargetItem.bIsStackable)
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

// =====================================================================
// 装备系统
// =====================================================================

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

    // 武器：同角色只能装备一把，先卸下旧武器
    if (Item.ItemCategory == EItemCategory::Weapon)
    {
        FItemInstance OldWeapon = GetEquippedWeapon(CharacterID);
        if (OldWeapon.ItemGUID.IsValid())
        {
            UnequipItem(OldWeapon.ItemGUID);
        }
    }

    // 圣遗物：同角色同部位只能装备一件，先卸下旧圣遗物
    if (Item.ItemCategory == EItemCategory::Artifact)
    {
        TArray<FItemInstance> CurrentArtifacts = GetEquippedArtifacts(CharacterID);
        for (const FItemInstance& Artifact : CurrentArtifacts)
        {
            if (Artifact.ArtifactData.Slot == Item.ArtifactData.Slot)
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
        if (Item.EquippedCharacterID == CharacterID)
        {
            Item.EquippedCharacterID = -1;
        }
    }
    OnInventoryUpdated.Broadcast();
}

FItemInstance UInventoryManagerSubsystem::GetEquippedWeapon(int32 CharacterID) const
{
    for (const FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemCategory == EItemCategory::Weapon && Item.EquippedCharacterID == CharacterID)
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
        if (Item.ItemCategory == EItemCategory::Artifact && Item.EquippedCharacterID == CharacterID)
        {
            Result.Add(Item);
        }
    }
    return Result;
}

// =====================================================================
// 使用/消耗系统
// =====================================================================

bool UInventoryManagerSubsystem::UseItem(FGuid ItemGUID, int32 TargetCharacterID, int32 UseAmount)
{
    int32 Index = FindIndexByGUID(ItemGUID);
    if (Index == INDEX_NONE) return false;

    FItemInstance& Item = InventoryItems[Index];
    const FItemData* ItemConfig = GetItemData(Item.ItemID);
    if (!ItemConfig) return false;

    // 校验：不可使用的物品
    if (ItemConfig->UseTargetType == EItemUseTarget::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品ID %d 不可使用。"), Item.ItemID);
        return false;
    }

    // 校验：需要选择角色但未指定
    if (ItemConfig->UseTargetType == EItemUseTarget::SelectCharacter && TargetCharacterID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品ID %d 需要指定目标角色。"), Item.ItemID);
        return false;
    }

    // 校验：数量不足
    if (Item.bIsStackable && Item.Count < UseAmount)
    {
        UE_LOG(LogTemp, Warning, TEXT("UseItem: 物品数量不足。"));
        return false;
    }

    // 应用使用效果 GE
    if (!ItemConfig->UseEffectClass.IsNull())
    {
        // TODO: 通过 TargetCharacterID 找到对应的 APlayerCharacter，获取 ASC 应用 GE
        // 当前框架中 Subsystem 无法直接访问 World 中的 Actor
        // 这部分逻辑应在调用方 (如 BackpackComponent 或 Lua 脚本) 中实现
        UE_LOG(LogTemp, Log, TEXT("UseItem: 物品ID %d 使用效果需要由调用方应用。"), Item.ItemID);
    }

    // 扣减数量
    if (Item.bIsStackable)
    {
        Item.Count -= UseAmount;
        if (Item.Count <= 0)
        {
            InventoryItems.RemoveAt(Index);
        }
    }
    else
    {
        // 不可堆叠物品使用后整件移除 (如经验书)
        InventoryItems.RemoveAt(Index);
    }

    OnItemUsed.Broadcast(ItemGUID, Item.ItemID, UseAmount);
    OnInventoryUpdated.Broadcast();
    return true;
}

// =====================================================================
// 查询接口
// =====================================================================

void UInventoryManagerSubsystem::GetItemsByCategory(EItemCategory Category, TArray<FItemInstance>& OutItems) const
{
    OutItems.Empty();
    for (const FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemCategory == Category)
        {
            OutItems.Add(Item);
        }
    }
}

void UInventoryManagerSubsystem::GetItemsByFilter(EItemCategory Category, EItemSortMode SortMode, EItemRarity RarityFilter, TArray<FItemInstance>& OutItems) const
{
    OutItems.Empty();

    for (const FItemInstance& Item : InventoryItems)
    {
        if (Item.ItemCategory != Category) continue;

        // 稀有度筛选 (Star1 作为 "全部" 的标记)
        if (RarityFilter != EItemRarity::Star1)
        {
            FItemData StaticData;
            if (GetItemStaticData(Item.ItemID, StaticData))
            {
                if (StaticData.ItemRarity != RarityFilter) continue;
            }
        }

        OutItems.Add(Item);
    }

    // 排序
    const_cast<UInventoryManagerSubsystem*>(this)->SortItems(OutItems, SortMode);
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
    const FItemData* FoundRow = GetItemData(ItemID);
    if (FoundRow)
    {
        OutItemData = *FoundRow;
        return true;
    }
    return false;
}
