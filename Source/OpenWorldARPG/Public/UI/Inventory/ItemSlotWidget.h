// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemData.h"
#include "Types/ItemInstance.h"
#include "ItemSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UItemSlotWidget;
class UInventoryManagerSubsystem;
struct FItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSlotClicked, int32, ItemArrayIndex, const FItemInstance&, ItemInstance, const FItemData&, ItemData, UItemSlotWidget*, ItemSlot);

UCLASS()
class OPENWORLDARPG_API UItemSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void UpdateSlotInfo();

    const FItemData& GetCachedItemData() const { return CachedItemData; }

    void SetSelectionState(bool bIsSelected);

    UFUNCTION()
    void OnSlotIconLoaded(FSoftObjectPath LoadedPath);

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnSlotClicked OnSlotClicked;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    FItemInstance ItemInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    int32 ItemArrayIndex = -1;

protected:
    UFUNCTION()
    void OnSlotButtonClicked();

protected:
    // --- UI 组件绑定 ---

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> SlotButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ItemImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ItemAmountText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> SelectionHighlightImage;

private:
    FItemData CachedItemData;
};