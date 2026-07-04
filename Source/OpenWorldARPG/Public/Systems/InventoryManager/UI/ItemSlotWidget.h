// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "ItemSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UItemSlotWidget;
class UItemObject;
class UInventoryViewModel;
class UInventoryManagerSubsystem;
struct FItemInstance;

/**
 * 物品槽位 UI (Dumb View)
 * 实现 IUserObjectListEntry 支持 UTileView 虚拟化复用，
 * 点击直接调用 ViewModel->SelectItem。
 */
UCLASS()
class OPENWORLDARPG_API UItemSlotWidget : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // --- IUserObjectListEntry 实现 ---

    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void UpdateSlotInfo();

    void SetSelectionState(bool bIsSelected);

protected:
    UFUNCTION()
    void OnSlotButtonClicked();

    UFUNCTION()
    void OnItemSelectionStateChanged(UItemObject* ItemObject, bool bIsSelected);

    // --- 控件绑定 ---

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> SlotButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ItemImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ItemAmountText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> SelectionHighlightImage;

private:
    UPROPERTY()
    TObjectPtr<UItemObject> BoundItemObject;

    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

    void OnSlotIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);
};
