// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "GameplayTagContainer.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
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

    // --- Tab 生成 ---

    void RefreshCategoryTabBox();
    void HandleSelectFirstCategoryTab();

    UFUNCTION()
    void HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory);

    // --- 按钮回调 ---

    UFUNCTION()
    void OnCloseButtonClicked();

    UFUNCTION()
    void OnDiscardButtonClicked();

protected:
    // --- 控件绑定 ---

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
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    UPROPERTY()
    TObjectPtr<UItemCategoryTabWidget> SelectedCategoryTab;
};
