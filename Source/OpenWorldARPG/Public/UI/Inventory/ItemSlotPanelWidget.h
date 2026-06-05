// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Types/SharedTypes.h"
#include "ItemSlotPanelWidget.generated.h"

class UWrapBox;
class UListView;
class UItemSlotWidget;

// 选中物品时广播，传递 GUID 用于丢弃/装备/使用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemSelectedInGrid, FGuid, SelectedItemGUID, int32, SelectedItemID, const FItemInstance&, SelectedItemInstance);

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    EItemSortMode SortMode = EItemSortMode::ByRarity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    EItemRarity RarityFilter = EItemRarity::Star1;

    UFUNCTION()
    void RefreshInventoryGrid();

    /** 切换排序模式 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void SetSortMode(EItemSortMode NewSortMode);

    /** 切换稀有度筛选 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Filter")
    void SetRarityFilter(EItemRarity NewFilter);

protected:
    UFUNCTION()
    void HandleSelectFirstSlot();

    UFUNCTION()
    void HandleSelectedSlot(int32 Index, const FItemInstance& Instance, const FItemData& Data, UItemSlotWidget* SlotWidget);

protected:
    /** 列表容器 (UE 5.2 使用 WrapBox，后续可升级为 TileView 虚拟化) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWrapBox> ItemWrapBox;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemSlotWidget> ItemSlotClass;

private:
    FGuid SelectedItemGUID;
    FItemInstance SelectedItemInstance;

    UPROPERTY()
    TObjectPtr<UItemSlotWidget> SelectedItemSlot;

    /** 缓存当前筛选后的物品列表 (供 TileView 回调使用) */
    UPROPERTY()
    TArray<FItemInstance> CachedFilteredItems;
};
