// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InventoryManager/UI/ItemObject.h"

void UItemObject::Initialize(const FItemInstance& InInstance, const FItemData* InStaticData)
{
    ItemInstance = InInstance;
    CachedStaticData = InStaticData;

    // 通知 UI 刷新
    if (OnItemDataChanged.IsBound())
    {
        OnItemDataChanged.Broadcast();
    }
}

FItemData UItemObject::GetItemStaticData() const
{
    return CachedStaticData ? *CachedStaticData : FItemData();
}

void UItemObject::SetSelected(bool bNewSelected)
{
    if (bIsSelected == bNewSelected)
    {
        return;
    }

    bIsSelected = bNewSelected;

    if (OnSelectionStateChanged.IsBound())
    {
        OnSelectionStateChanged.Broadcast(this, bIsSelected);
    }
}
