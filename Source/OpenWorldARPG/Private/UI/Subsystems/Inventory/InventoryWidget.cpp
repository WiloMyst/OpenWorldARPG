// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/InventoryWidget.h"
#include "UI/Subsystems/Inventory/ItemSlotPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemDetailPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemCategoryTabWidget.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Characters/PlayerCharacter.h"
#include "Components/InteractionComponent.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 自动从 GameAssetManagerSubsystem 加载 CategoryDataTable (不再依赖蓝图手动配置)
    if (!CategoryDataTable)
    {
        if (UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>())
        {
            CategoryDataTable = AssetManager->GetInventoryCategoryTabDataTable();
        }
    }

    // 1. 绑定按钮事件
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnCloseButtonClicked);
    }
    if (DiscardButton)
    {
        DiscardButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnDiscardButtonClicked);
    }

    // 2. 绑定格子容器的点击委托 (GUID 驱动)
    if (WBP_ItemSlotPanel)
    {
        WBP_ItemSlotPanel->OnItemSelectedInGrid.AddDynamic(this, &UInventoryWidget::HandleOnItemSelectedInGrid);
    }

    // 3. 对应图1右侧：初始化并选中首个 Tab
    RefreshCategoryTabBox();
    HandleSelectFirstCategoryTab();
}

void UInventoryWidget::RefreshCategoryTabBox()
{
    if (!CategoryBox || !CategoryDataTable || !CategoryTabClass) return;

    // 清除旧子项
    CategoryBox->ClearChildren();
    SelectedCategoryTab = nullptr;

    // 对应图3、4：遍历 DataTable 生成 Tab
    for (auto& Pair : CategoryDataTable->GetRowMap())
    {
        FInventoryCategoryTabData* RowData = (FInventoryCategoryTabData*)Pair.Value;
        if (RowData)
        {
            UItemCategoryTabWidget* NewTab = CreateWidget<UItemCategoryTabWidget>(this, CategoryTabClass);
            if (NewTab)
            {
                NewTab->CategoryTabData = *RowData;
                NewTab->OnTabClicked.AddDynamic(this, &UInventoryWidget::HandleOnTabClicked);
                CategoryBox->AddChild(NewTab);
            }
        }
    }
}

void UInventoryWidget::HandleSelectFirstCategoryTab()
{
    if (CategoryBox && CategoryBox->GetChildrenCount() > 0)
    {
        UItemCategoryTabWidget* FirstTab = Cast<UItemCategoryTabWidget>(CategoryBox->GetChildAt(0));
        if (FirstTab)
        {
            HandleSelectCategoryTab(FirstTab);
        }
    }
}

void UInventoryWidget::HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory)
{
    if (WBP_ItemSlotPanel)
    {
        WBP_ItemSlotPanel->ItemCategory = NewTabCategory;
    }
    HandleSelectCategoryTab(NewCategoryTab);
}

void UInventoryWidget::HandleSelectCategoryTab(UItemCategoryTabWidget* NewCategoryTab)
{
    if (SelectedCategoryTab)
    {
        SelectedCategoryTab->SetTabSelectedState(false);
    }

    SelectedCategoryTab = NewCategoryTab;

    if (SelectedCategoryTab)
    {
        SelectedCategoryTab->SetTabSelectedState(true);
    }

    if (WBP_ItemDetailPanel)
    {
        WBP_ItemDetailPanel->SetRenderOpacity(0.0f);
    }

    if (WBP_ItemSlotPanel)
    {
        WBP_ItemSlotPanel->RefreshInventoryGrid();
    }
}

void UInventoryWidget::HandleOnItemSelectedInGrid(FGuid SelectedItemGUID, int32 SelectedItemID, const FItemInstance& SelectedItemInstance)
{
    CachedSelectedItemGUID = SelectedItemGUID;

    if (WBP_ItemDetailPanel)
    {
        WBP_ItemDetailPanel->UpdateDetails(SelectedItemInstance);
        WBP_ItemDetailPanel->SetRenderOpacity(1.0f);
    }
}

void UInventoryWidget::OnCloseButtonClicked()
{
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }
}

void UInventoryWidget::OnDiscardButtonClicked()
{
    if (!CachedSelectedItemGUID.IsValid()) return;

    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwningPlayerPawn()))
    {
        if (UInteractionComponent* Backpack = PlayerChar->GetComponentByClass<UInteractionComponent>())
        {
            // 使用 GUID 驱动的丢弃，不再依赖数组索引
            Backpack->DropItemByGUID(CachedSelectedItemGUID, 1);
        }
    }
}
