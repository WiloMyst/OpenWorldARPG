// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Types/ItemInstance.h"
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
 *
 * 【MVVM 架构：纯表现层】
 * 1. 实现 IUserObjectListEntry 接口，支持 UTileView 虚拟化复用
 * 2. 在 NativeOnListItemObjectSet 中绑定 UItemObject 的 OnSelectionStateChanged 委托
 * 3. 点击事件直接调用 ViewModel->SelectItem，不层层传递委托
 * 4. 异步加载句柄缓存，NativeDestruct 中取消未完成的加载
 * 5. 使用 SetVisibility 替代 SetRenderOpacity 控制显示隐藏
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

    /** UItemObject 选中状态变化回调 */
    UFUNCTION()
    void OnItemSelectionStateChanged(UItemObject* ItemObject, bool bIsSelected);

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
    /** 当前绑定的 UItemObject (用于解绑旧委托) */
    UPROPERTY()
    TObjectPtr<UItemObject> BoundItemObject;

    /** 异步加载句柄，用于取消未完成的加载 */
    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

    /** 图标加载完成回调（强类型绑定） */
    void OnSlotIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);
};
