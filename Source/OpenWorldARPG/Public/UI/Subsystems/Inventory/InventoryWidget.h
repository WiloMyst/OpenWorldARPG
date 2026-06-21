// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "GameplayTagContainer.h"
#include "Types/ItemTypes.h"
#include "Types/ItemInstance.h"
#include "InventoryWidget.generated.h"

class UItemSlotPanelWidget;
class UItemDetailPanelWidget;
class UItemCategoryTabWidget;
class UInventoryViewModel;
class UVerticalBox;
class UButton;
class UDataTable;
struct FItemInstance;
struct FItemData;

/** 背包主 UI */
UCLASS()
class OPENWORLDARPG_API UInventoryWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- Tab 生成 (纯表现层) ---

    void RefreshCategoryTabBox();

    void HandleSelectFirstCategoryTab();

    UFUNCTION()
    void HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory);

    // --- 按钮响应 (仅调用 VM) ---

    UFUNCTION()
    void OnCloseButtonClicked();

    UFUNCTION()
    void OnDiscardButtonClicked();

protected:
    // --- UI 组件绑定 ---

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UItemSlotPanelWidget> WBP_ItemSlotPanel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UItemDetailPanelWidget> WBP_ItemDetailPanel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> CategoryBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> DiscardButton;

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TObjectPtr<UDataTable> CategoryDataTable;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemCategoryTabWidget> CategoryTabClass;

private:
    /** 视图模型 (MVVM 核心中介层) */
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    UPROPERTY()
    TObjectPtr<UItemCategoryTabWidget> SelectedCategoryTab;
};
