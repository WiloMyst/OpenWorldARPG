// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/InventoryWidget.h"
#include "UI/Subsystems/Inventory/ItemSlotPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemDetailPanelWidget.h"
#include "UI/Subsystems/Inventory/ItemCategoryTabWidget.h"
#include "UI/Subsystems/Inventory/InventoryViewModel.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"

void UInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 1. 实例化核心 ViewModel
    ViewModel = NewObject<UInventoryViewModel>(this);

    // 2. 初始化 VM 数据
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UInventoryManagerSubsystem* InventorySubsystem = LocalPlayer->GetSubsystem<UInventoryManagerSubsystem>())
            {
                ViewModel->InitializeViewModel(InventorySubsystem);
            }
        }
    }

    // 3. 将 ViewModel 下发给所有子面板，接通数据流
    if (WBP_ItemSlotPanel)
    {
        WBP_ItemSlotPanel->SetViewModel(ViewModel);
    }
    if (WBP_ItemDetailPanel)
    {
        WBP_ItemDetailPanel->SetViewModel(ViewModel);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnCloseButtonClicked);
    }
    if (DiscardButton)
    {
        DiscardButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnDiscardButtonClicked);
    }
}

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

    // 生成 Tab 并选中首个（触发 VM 的 SelectCategory）
    RefreshCategoryTabBox();
    HandleSelectFirstCategoryTab();
}

void UInventoryWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UInventoryWidget::RefreshCategoryTabBox()
{
    if (!CategoryBox || !CategoryDataTable || !CategoryTabClass) return;

    // 清除旧子项
    CategoryBox->ClearChildren();
    SelectedCategoryTab = nullptr;

    // 遍历 DataTable 生成 Tab
    for (auto& Pair : CategoryDataTable->GetRowMap())
    {
        FInventoryCategoryTabData* RowData = (FInventoryCategoryTabData*)Pair.Value;
        if (RowData)
        {
            UItemCategoryTabWidget* NewTab = CreateWidget<UItemCategoryTabWidget>(this, CategoryTabClass);
            if (NewTab)
            {
                NewTab->CategoryTabData = *RowData;
                NewTab->UpdateTabInfo();
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
            HandleOnTabClicked(FirstTab, FirstTab->CategoryTabData.TabCategory);
        }
    }
}

void UInventoryWidget::HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory)
{
    // 委托给 ViewModel 处理分类切换 (VM 负责过滤数据并广播)
    if (ViewModel)
    {
        ViewModel->SelectCategory(NewTabCategory);
    }
}

void UInventoryWidget::OnCloseButtonClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->CloseTopUI();
        }
    }
}

void UInventoryWidget::OnDiscardButtonClicked()
{
    if (ViewModel)
    {
        ViewModel->RequestDiscardSelectedItem();
    }
}