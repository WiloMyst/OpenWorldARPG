// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "GameplayTagContainer.h"
#include "Types/SharedTypes.h"
#include "InventoryWidget.generated.h"

class UItemSlotPanelWidget;
class UItemDetailPanelWidget;
class UItemCategoryTabWidget;
class UHorizontalBox;
class UButton;
class UDataTable;
struct FItemInstance;
struct FItemData;

UCLASS()
class OPENWORLDARPG_API UInventoryWidget : public UBaseMenuWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    // ==========================================
    // 核心流转逻辑
    // ==========================================

    // 对应图3、图4：读取数据表并生成所有分类 Tab
    void RefreshCategoryTabBox();

    // 对应图5 (右侧)：选中首个分类 Tab
    void HandleSelectFirstCategoryTab();

    // 对应图6：处理分类 Tab 的切换与状态重置
    UFUNCTION()
    void HandleSelectCategoryTab(UItemCategoryTabWidget* NewCategoryTab);

    // 对应图4 (右侧)：响应 Tab 的点击事件
    UFUNCTION()
    void HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory);

    // 对应图1：响应格子的点击事件，更新详情面板
    UFUNCTION()
    void HandleOnItemSelectedInGrid(int32 SelectedItemID, const FItemInstance& SelectedItemInstance, const FItemData& SelectedItemData);

    // ==========================================
    // 按钮响应逻辑
    // ==========================================

    UFUNCTION()
    void OnCloseButtonClicked();

    UFUNCTION()
    void OnDiscardButtonClicked();

protected:
    // ==========================================
    // UI 组件绑定 (BindWidget)
    // ==========================================

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UItemSlotPanelWidget> WBP_ItemSlotPanel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UItemDetailPanelWidget> WBP_ItemDetailPanel;

    // 对应截图中的 Category Box (假设是 HorizontalBox，如果是 WrapBox 请替换)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> CategoryBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> DiscardButton;

    // ==========================================
    // 配置与状态变量
    // ==========================================

    // 蓝图配置：分类 Tab 的数据表 (DT_InventoryCategoryTabData)
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TObjectPtr<UDataTable> CategoryDataTable;

    // 蓝图配置：动态生成的分类 Tab 类
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemCategoryTabWidget> CategoryTabClass;

private:
    // 状态记录
    UPROPERTY()
    TObjectPtr<UItemCategoryTabWidget> SelectedCategoryTab;

    int32 CachedSelectedItemID = -1;
};