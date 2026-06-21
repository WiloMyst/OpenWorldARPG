// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/InventoryViewModel.h"
#include "UI/Subsystems/Inventory/ItemObject.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Components/InteractionComponent.h"
#include "Algo/Sort.h"

void UInventoryViewModel::InitializeViewModel(UInventoryManagerSubsystem* Subsystem)
{
    InventoryManager = Subsystem;
    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &UInventoryViewModel::HandleInventoryUpdated);
    }
}

void UInventoryViewModel::SelectCategory(EItemCategory NewCategory)
{
    if (CurrentCategory == NewCategory && FilteredItemObjects.Num() > 0)
    {
        return;
    }

    CurrentCategory = NewCategory;
    RebuildFilteredItems();

    // 切换分类后自动选中第一个物品
    if (FilteredItemObjects.Num() > 0)
    {
        SetSelectedItem(FilteredItemObjects[0]);
    }
    else
    {
        SetSelectedItem(nullptr);
    }

    if (OnInventoryListUpdated.IsBound())
    {
        OnInventoryListUpdated.Broadcast();
    }
}

void UInventoryViewModel::SelectItem(UItemObject* ItemObj)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] ViewModel 收到 SelectItem 请求。"));

    if (!ItemObj)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] ViewModel 收到 SelectItem 请求，但 ItemObj 为空！"));
        return;
    }

    SetSelectedItem(ItemObj);
}

void UInventoryViewModel::RequestDiscardSelectedItem()
{
    if (!SelectedItemObject || !InventoryManager)
    {
        return;
    }

    // 丢弃的逻辑应该由 InventoryManager（Model层）直接提供接口
    InventoryManager->DiscardItemByGUID(SelectedItemObject->ItemInstance.ItemGUID, 1);
}

void UInventoryViewModel::SetSortMode(EItemSortMode NewSortMode)
{
    if (SortMode == NewSortMode)
    {
        return;
    }

    SortMode = NewSortMode;
    RebuildFilteredItems();

    if (OnInventoryListUpdated.IsBound())
    {
        OnInventoryListUpdated.Broadcast();
    }
}

void UInventoryViewModel::SetRarityFilter(EItemRarity NewFilter)
{
    if (RarityFilter == NewFilter)
    {
        return;
    }

    RarityFilter = NewFilter;
    RebuildFilteredItems();

    if (OnInventoryListUpdated.IsBound())
    {
        OnInventoryListUpdated.Broadcast();
    }
}

void UInventoryViewModel::HandleInventoryUpdated()
{
    RebuildFilteredItems();

    // 数据变化后检查选中物品是否仍然有效
    if (SelectedItemObject)
    {
        const FGuid& SelectedGUID = SelectedItemObject->ItemInstance.ItemGUID;
        bool bStillExists = false;
        for (UItemObject* ItemObj : FilteredItemObjects)
        {
            if (ItemObj && ItemObj->ItemInstance.ItemGUID == SelectedGUID)
            {
                bStillExists = true;
                break;
            }
        }

        if (!bStillExists)
        {
            // 选中物品已不在列表中，切换到第一个
            SetSelectedItem(FilteredItemObjects.Num() > 0 ? FilteredItemObjects[0] : nullptr);
        }
    }
    else if (FilteredItemObjects.Num() > 0)
    {
        SetSelectedItem(FilteredItemObjects[0]);
    }

    if (OnInventoryListUpdated.IsBound())
    {
        OnInventoryListUpdated.Broadcast();
    }
}

UItemObject* UInventoryViewModel::AcquireItemObject()
{
    // 从对象池末尾取出一个可复用的 UItemObject
    if (ItemObjectPool.Num() > 0)
    {
        UItemObject* PooledObj = ItemObjectPool.Pop();
        return PooledObj;
    }
    return NewObject<UItemObject>(this);
}

void UInventoryViewModel::RebuildFilteredItems()
{
    if (!InventoryManager)
    {
        return;
    }

    // 从 Model 获取筛选+排序后的数据
    TArray<FItemInstance> FilteredItems;
    InventoryManager->GetItemsByFilter(CurrentCategory, RarityFilter, FilteredItems);

    // 暂存并清空选中项，防止指向对象池中的脏数据
    FGuid OldSelectedGUID;
    if (SelectedItemObject)
    {
        OldSelectedGUID = SelectedItemObject->ItemInstance.ItemGUID;
        SelectedItemObject = nullptr; 
    }

    // 将当前的 FilteredItemObjects 回收到对象池
    for (UItemObject* ItemObj : FilteredItemObjects)
    {
        if (ItemObj)
        {
            // 清除选中状态（不广播，因为 Widget 会被 SetListItems 重新绑定）
            ItemObj->bIsSelected = false;
            ItemObjectPool.Add(ItemObj);
        }
    }
    FilteredItemObjects.Reset();

    const int32 RequiredCount = FilteredItems.Num();
    FilteredItemObjects.Reserve(RequiredCount);

    for (int32 i = 0; i < RequiredCount; ++i)
    {
        UItemObject* ItemObj = AcquireItemObject();
        if (ItemObj)
        {
            const FItemData* StaticData = InventoryManager->GetItemData(FilteredItems[i].ItemID);
            ItemObj->Initialize(FilteredItems[i], StaticData); // 调用你之前重构好的 Initialize

            // 匹配暂存的 GUID，正确还原选中状态
            if (OldSelectedGUID.IsValid() && ItemObj->ItemInstance.ItemGUID == OldSelectedGUID)
            {
                ItemObj->bIsSelected = true;
                SelectedItemObject = ItemObj; // 恢复正确的选中指针
            }
            else
            {
                ItemObj->bIsSelected = false;
            }
            
            FilteredItemObjects.Add(ItemObj);
        }
    }

    // 拿到全新数据后，由 ViewModel 进行排序
    SortItemObjects();

    // 排序后赋予真正的数组索引
    for (int32 i = 0; i < FilteredItemObjects.Num(); ++i)
    {
        FilteredItemObjects[i]->ItemArrayIndex = i;
    }
}

void UInventoryViewModel::SortItemObjects()
{
    if (FilteredItemObjects.IsEmpty()) return;

    // ViewModel 使用 Algo::Sort 就地排序对象数组
    Algo::Sort(FilteredItemObjects, [this](const UItemObject* A, const UItemObject* B)
        {
            // 判空保护
            if (!A || !B) return false;
            if (!A->CachedStaticData || !B->CachedStaticData) return false;

            switch (SortMode)
            {
            case EItemSortMode::ByRarity:
                // 先按稀有度降序，稀有度相同按 ID 升序（保证同样物品排在一起）
                if (A->CachedStaticData->ItemRarity != B->CachedStaticData->ItemRarity)
                    return A->CachedStaticData->ItemRarity > B->CachedStaticData->ItemRarity;
                return A->ItemInstance.ItemID < B->ItemInstance.ItemID;

            case EItemSortMode::ByName:
                // 按名称字母排序
                return A->CachedStaticData->ItemName.ToString() < B->CachedStaticData->ItemName.ToString();

            case EItemSortMode::ByTime:
                // 按获取时间降序（最新的在前面）
                return A->ItemInstance.AcquiredTime > B->ItemInstance.AcquiredTime;

                // 你可以随时在此补充按等级、按属性排序的逻辑
            default:
                return false;
            }
        });
}

void UInventoryViewModel::SetSelectedItem(UItemObject* NewItem)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] ViewModel 正在设置选中物品..."));

    // 清除旧选中项
    if (SelectedItemObject)
    {
        SelectedItemObject->SetSelected(false);
    }

    SelectedItemObject = NewItem;

    // 设置新选中项
    if (SelectedItemObject)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] 成功设置新的选中项 ItemID: %d"), SelectedItemObject->ItemInstance.ItemID);
        SelectedItemObject->SetSelected(true);
    }

    if (OnSelectedItemChanged.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] ViewModel 正在广播 OnSelectedItemChanged..."));
        OnSelectedItemChanged.Broadcast(SelectedItemObject);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] 致命错误：OnSelectedItemChanged 没有任何绑定！(DetailPanel没绑上?)"));
    }
}
