// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "GameplayTagContainer.h"
#include "Types/ItemTypes.h"
#include "Types/ItemInstance.h"
#include "InventoryWidget.generated.h"

class UItemSlotPanelWidget;
class UItemDetailPanelWidget;
class UItemCategoryTabWidget;
class UVerticalBox;
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

    // --- 核心流转逻辑 ---

    void RefreshCategoryTabBox();

    void HandleSelectFirstCategoryTab();

    UFUNCTION()
    void HandleSelectCategoryTab(UItemCategoryTabWidget* NewCategoryTab);

    UFUNCTION()
    void HandleOnTabClicked(UItemCategoryTabWidget* NewCategoryTab, EItemCategory NewTabCategory);

    UFUNCTION()
    void HandleOnItemSelectedInGrid(FGuid SelectedItemGUID, int32 SelectedItemID, const FItemInstance& SelectedItemInstance);

    // --- 按钮响应 ---

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
    UPROPERTY()
    TObjectPtr<UItemCategoryTabWidget> SelectedCategoryTab;

    FGuid CachedSelectedItemGUID;
};