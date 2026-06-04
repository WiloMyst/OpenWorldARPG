// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Types/SharedTypes.h"
#include "ItemSlotPanelWidget.generated.h"

class UWrapBox;
class UItemSlotWidget;

// 选中物品时广播，传递 ItemID 用于丢弃时实时查找索引
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemSelectedInGrid, int32, SelectedItemID, const FItemInstance&, SelectedItemInstance, const FItemData&, SelectedItemData);

UCLASS()
class OPENWORLDARPG_API UItemSlotPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemSelectedInGrid OnItemSelectedInGrid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    EItemCategory ItemCategory;

    UFUNCTION()
    void RefreshInventoryGrid();

protected:
    UFUNCTION()
    void HandleSelectFirstSlot();

    UFUNCTION()
    void HandleSelectedSlot(int32 Index, const FItemInstance& Instance, const FItemData& Data, UItemSlotWidget* SlotWidget);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWrapBox> WrapBox;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemSlotWidget> ItemSlotClass;

private:
    int32 SelectedItemID = -1;
    FItemInstance SelectedItemInstance;

    UPROPERTY()
    TObjectPtr<UItemSlotWidget> SelectedItemSlot;
};
