// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/ItemTypes.h"
#include "Types/ItemInstance.h"
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

    /** 接收父级传入的 ViewModel 并绑定委托 */
    void SetViewModel(UInventoryViewModel* InViewModel);

    /** 获取 ViewModel (供子 Widget 调用 VM 命令) */
    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    UInventoryViewModel* GetViewModel() const { return ViewModel; }

    /** 切换排序模式 (转发给 VM) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void SetSortMode(EItemSortMode NewSortMode);

    /** 切换稀有度筛选 (转发给 VM) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Filter")
    void SetRarityFilter(EItemRarity NewFilter);

protected:
    UFUNCTION()
    void HandleInventoryListUpdated();

protected:
    /** 虚拟化列表容器 (支持 Widget 复用，消除 GC 峰值) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTileView> ItemTileView;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    TSubclassOf<UItemSlotWidget> ItemSlotClass;

private:
    /** ViewModel 引用 (由父级 InventoryWidget 传入) */
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;
};
