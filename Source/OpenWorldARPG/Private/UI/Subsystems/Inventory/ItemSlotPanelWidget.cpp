// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/ItemSlotPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemSlotWidget.h"
#include "Components/WrapBox.h"
#include "Managers/InventoryManagerSubsystem.h"

void UItemSlotPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &UItemSlotPanelWidget::RefreshInventoryGrid);
    }

    // 不在 NativeConstruct 中主动刷新！
    // 初始刷新由 InventoryWidget::HandleSelectCategoryTab → RefreshInventoryGrid 触发
    // 避免在 ItemCategory 未设置时创建空白格子
}

void UItemSlotPanelWidget::NativeDestruct()
{
    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UItemSlotPanelWidget::RefreshInventoryGrid);
    }
    Super::NativeDestruct();
}

void UItemSlotPanelWidget::RefreshInventoryGrid()
{
    if (!ItemWrapBox || !ItemSlotClass) return;

    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (!InventoryManager) return;

    SelectedItemSlot = nullptr;
    SelectedItemGUID = FGuid();

    // 使用 Subsystem 的筛选+排序接口
    CachedFilteredItems.Empty();
    InventoryManager->GetItemsByFilter(ItemCategory, SortMode, RarityFilter, CachedFilteredItems);

    // 清空旧条目并重建
    ItemWrapBox->ClearChildren();
    for (int32 i = 0; i < CachedFilteredItems.Num(); ++i)
    {
        UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, ItemSlotClass);
        if (NewSlot)
        {
            NewSlot->ItemInstance = CachedFilteredItems[i];
            NewSlot->ItemArrayIndex = i;
            NewSlot->OnSlotClicked.AddDynamic(this, &UItemSlotPanelWidget::HandleSelectedSlot);
            ItemWrapBox->AddChildToWrapBox(NewSlot);
        }
    }

    // 只在有物品时才选中第一个
    if (CachedFilteredItems.Num() > 0)
    {
        HandleSelectFirstSlot();
    }
}

void UItemSlotPanelWidget::SetSortMode(EItemSortMode NewSortMode)
{
    SortMode = NewSortMode;
    RefreshInventoryGrid();
}

void UItemSlotPanelWidget::SetRarityFilter(EItemRarity NewFilter)
{
    RarityFilter = NewFilter;
    RefreshInventoryGrid();
}

void UItemSlotPanelWidget::HandleSelectFirstSlot()
{
    if (ItemWrapBox && ItemWrapBox->GetChildrenCount() > 0)
    {
        UItemSlotWidget* FirstSlot = Cast<UItemSlotWidget>(ItemWrapBox->GetChildAt(0));
        if (FirstSlot)
        {
            HandleSelectedSlot(FirstSlot->ItemArrayIndex, FirstSlot->ItemInstance, FirstSlot->GetCachedItemData(), FirstSlot);
        }
    }
}

void UItemSlotPanelWidget::HandleSelectedSlot(int32 Index, const FItemInstance& Instance, const FItemData& Data, UItemSlotWidget* SlotWidget)
{
    // 使用 GUID 而非 ItemID 作为选中标识
    SelectedItemGUID = Instance.ItemGUID;
    SelectedItemInstance = Instance;

    if (SelectedItemSlot)
    {
        SelectedItemSlot->SetSelectionState(false);
    }

    SelectedItemSlot = SlotWidget;
    if (SelectedItemSlot)
    {
        SelectedItemSlot->SetSelectionState(true);
    }

    if (OnItemSelectedInGrid.IsBound())
    {
        OnItemSelectedInGrid.Broadcast(SelectedItemGUID, Instance.ItemID, SelectedItemInstance);
    }
}
