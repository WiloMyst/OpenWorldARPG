// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Inventory/ItemSlotPanelWidget.h"
#include "UI/Inventory/ItemSlotWidget.h"
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
    if (!WrapBox || !ItemSlotClass) return;

    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (!InventoryManager) return;

    WrapBox->ClearChildren();
    SelectedItemSlot = nullptr;
    SelectedItemID = -1;

    // 使用 Subsystem 的分类查询接口，避免本地重复过滤逻辑
    TArray<FItemInstance> FilteredItems = InventoryManager->GetItemsByCategory(ItemCategory);

    for (int32 i = 0; i < FilteredItems.Num(); ++i)
    {
        const FItemInstance& Instance = FilteredItems[i];

        UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, ItemSlotClass);
        if (NewSlot)
        {
            NewSlot->ItemInstance = Instance;
            NewSlot->ItemArrayIndex = i;

            NewSlot->OnSlotClicked.AddDynamic(this, &UItemSlotPanelWidget::HandleSelectedSlot);

            WrapBox->AddChildToWrapBox(NewSlot);
        }
    }

    HandleSelectFirstSlot();
}

void UItemSlotPanelWidget::HandleSelectFirstSlot()
{
    if (WrapBox && WrapBox->GetChildrenCount() > 0)
    {
        UItemSlotWidget* FirstSlot = Cast<UItemSlotWidget>(WrapBox->GetChildAt(0));
        if (FirstSlot)
        {
            HandleSelectedSlot(FirstSlot->ItemArrayIndex, FirstSlot->ItemInstance, FirstSlot->GetCachedItemData(), FirstSlot);
        }
    }
}

void UItemSlotPanelWidget::HandleSelectedSlot(int32 Index, const FItemInstance& Instance, const FItemData& Data, UItemSlotWidget* SlotWidget)
{
    // 缓存 ItemID 而非全局索引，丢弃时通过 ItemID 实时查找
    SelectedItemID = Instance.ItemID;
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
        OnItemSelectedInGrid.Broadcast(SelectedItemID, SelectedItemInstance, Data);
    }
}
