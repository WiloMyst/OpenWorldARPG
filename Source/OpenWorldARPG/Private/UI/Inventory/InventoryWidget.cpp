// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Inventory/InventoryWidget.h"
#include "UI/Inventory/ItemSlotPanelWidget.h"
#include "UI/Inventory/ItemDetailPanelWidget.h"
#include "UI/Inventory/ItemCategoryTabWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Characters/PlayerCharacter.h"
#include "Components/BackpackComponent.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 1. 绑定按钮事件
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnCloseButtonClicked);
    }
    if (DiscardButton)
    {
        DiscardButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnDiscardButtonClicked);
    }

    // 2. 绑定格子容器的点击委托
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
    // 这是 UE C++ 遍历数据表的最标准写法
    for (auto& Pair : CategoryDataTable->GetRowMap())
    {
        // 将行数据强转为你的结构体
        FInventoryCategoryTabData* RowData = (FInventoryCategoryTabData*)Pair.Value;
        if (RowData)
        {
            UItemCategoryTabWidget* NewTab = CreateWidget<UItemCategoryTabWidget>(this, CategoryTabClass);
            if (NewTab)
            {
                // 赋值并绑定事件
                NewTab->CategoryTabData = *RowData;
                NewTab->OnTabClicked.AddDynamic(this, &UInventoryWidget::HandleOnTabClicked);

                // 添加到面板中
                CategoryBox->AddChild(NewTab);
            }
        }
    }
}

void UInventoryWidget::HandleSelectFirstCategoryTab()
{
    if (CategoryBox && CategoryBox->GetChildrenCount() > 0)
    {
        // 获取第一个子控件并触发选中
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
        // 传递 Category 给 Slot Panel
        WBP_ItemSlotPanel->ItemCategory = NewTabCategory;
    }

    // 继续执行选中逻辑
    HandleSelectCategoryTab(NewCategoryTab);
}

void UInventoryWidget::HandleSelectCategoryTab(UItemCategoryTabWidget* NewCategoryTab)
{
    // 对应图6：状态清理与交接
    if (SelectedCategoryTab)
    {
        SelectedCategoryTab->SetTabSelectedState(false); // 取消旧高亮
    }

    SelectedCategoryTab = NewCategoryTab;

    if (SelectedCategoryTab)
    {
        SelectedCategoryTab->SetTabSelectedState(true); // 开启新高亮
    }

    // 隐藏详情面板
    if (WBP_ItemDetailPanel)
    {
        WBP_ItemDetailPanel->SetRenderOpacity(0.0f);
    }

    // 通知格子容器刷新
    if (WBP_ItemSlotPanel)
    {
        WBP_ItemSlotPanel->RefreshInventoryGrid();
    }
}

void UInventoryWidget::HandleOnItemSelectedInGrid(int32 SelectedItemID, const FItemInstance& SelectedItemInstance, const FItemData& SelectedItemData)
{
    CachedSelectedItemID = SelectedItemID;

    if (WBP_ItemDetailPanel)
    {
        WBP_ItemDetailPanel->UpdateDetails(SelectedItemInstance);
        WBP_ItemDetailPanel->SetRenderOpacity(1.0f);
    }
}

void UInventoryWidget::OnCloseButtonClicked()
{
    // 对应图1：呼叫 UIManager 子系统关闭 UI
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }
}

void UInventoryWidget::OnDiscardButtonClicked()
{
    if (CachedSelectedItemID < 0) return;

    // 通过 ItemID 实时查找全局数组索引，避免索引错位
    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (!InventoryManager) return;

    const TArray<FItemInstance>& AllItems = InventoryManager->GetAllInventoryItems();
    int32 GlobalIndex = -1;
    for (int32 i = 0; i < AllItems.Num(); ++i)
    {
        if (AllItems[i].ItemID == CachedSelectedItemID)
        {
            GlobalIndex = i;
            break;
        }
    }

    if (GlobalIndex < 0) return;

    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwningPlayerPawn()))
    {
        if (UBackpackComponent* Backpack = PlayerChar->GetComponentByClass<UBackpackComponent>())
        {
            Backpack->DropItem(GlobalIndex, 1);
        }
    }
}