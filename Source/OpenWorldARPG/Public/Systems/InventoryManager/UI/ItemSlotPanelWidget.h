// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "ItemSlotPanelWidget.generated.h"

class UTileView;
class UItemSlotWidget;
class UItemObject;
class UInventoryViewModel;

/** 物品槽位面板 UI */
UCLASS()
class OPENWORLDARPG_API UItemSlotPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeDestruct() override;

    void SetViewModel(UInventoryViewModel* InViewModel);

    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    UInventoryViewModel* GetViewModel() const { return ViewModel; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void SetSortMode(EItemSortMode NewSortMode);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Filter")
    void SetRarityFilter(EItemRarity NewFilter);

protected:
    UFUNCTION()
    void HandleInventoryListUpdated();

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTileView> ItemTileView;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemSlotWidget> ItemSlotClass;

private:
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;
};
