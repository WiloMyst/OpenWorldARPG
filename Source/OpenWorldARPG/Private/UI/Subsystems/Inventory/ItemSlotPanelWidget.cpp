// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/ItemSlotPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemSlotWidget.h"
#include "UI/Subsystems/Inventory/ItemObject.h"
#include "UI/Subsystems/Inventory/InventoryViewModel.h"
#include "Components/TileView.h"

void UItemSlotPanelWidget::NativeDestruct()
{
    // 生命周期安全：解绑 ViewModel 委托，防止野指针崩溃
    if (ViewModel)
    {
        ViewModel->OnInventoryListUpdated.RemoveDynamic(this, &UItemSlotPanelWidget::HandleInventoryListUpdated);
    }
    Super::NativeDestruct();
}

void UItemSlotPanelWidget::SetViewModel(UInventoryViewModel* InViewModel)
{
    // 如果已有旧 ViewModel，先解绑
    if (ViewModel)
    {
        ViewModel->OnInventoryListUpdated.RemoveDynamic(this, &UItemSlotPanelWidget::HandleInventoryListUpdated);
    }

    ViewModel = InViewModel;

    if (ViewModel)
    {
        // 监听 VM 的列表更新广播
        ViewModel->OnInventoryListUpdated.AddDynamic(this, &UItemSlotPanelWidget::HandleInventoryListUpdated);

        // 立即刷新一次
        HandleInventoryListUpdated();
    }
}

void UItemSlotPanelWidget::HandleInventoryListUpdated()
{
    if (!ItemTileView || !ViewModel)
    {
        return;
    }

    // 直接从 ViewModel 获取数据源，塞给 TileView 渲染
    ItemTileView->SetListItems(ViewModel->GetFilteredItemObjects());
}

void UItemSlotPanelWidget::SetSortMode(EItemSortMode NewSortMode)
{
    if (ViewModel)
    {
        ViewModel->SetSortMode(NewSortMode);
    }
}

void UItemSlotPanelWidget::SetRarityFilter(EItemRarity NewFilter)
{
    if (ViewModel)
    {
        ViewModel->SetRarityFilter(NewFilter);
    }
}
