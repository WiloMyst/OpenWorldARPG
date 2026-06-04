// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Types/SharedTypes.h" 
#include "ItemSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UItemSlotWidget;
struct FItemInstance;

// ==========================================
// 委托声明：当格子被点击时广播
// 对应蓝图的 "调用 On Slot Clicked"
// ==========================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSlotClicked, int32, ItemArrayIndex, const FItemInstance&, ItemInstance, const FItemData&, ItemData, UItemSlotWidget*, ItemSlot);

UCLASS()
class OPENWORLDARPG_API UItemSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 相当于蓝图的 Event Construct
    virtual void NativeConstruct() override;

    // 对应蓝图的 "更新 Slot信息"
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void UpdateSlotInfo();

    // 提供给外部获取缓存数据的接口
    const FItemData& GetCachedItemData() const { return CachedItemData; }

    // 控制选中高亮框的显隐
    void SetSelectionState(bool bIsSelected);

    // 异步图标加载完成回调
    UFUNCTION()
    void OnSlotIconLoaded(FSoftObjectPath LoadedPath);

    // 暴露给外部系统的点击事件分发器
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnSlotClicked OnSlotClicked;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    FItemInstance ItemInstance;

    /** 物品在全局数组中的索引 (每次 RefreshInventoryGrid 时由 Panel 赋值) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    int32 ItemArrayIndex = -1;

protected:
    // 按钮点击的回调函数
    UFUNCTION()
    void OnSlotButtonClicked();

protected:
    // ==========================================
    // UI 组件绑定 (BindWidget)
    // 变量名必须与蓝图中的组件名称完全一致！
    // ==========================================

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> SlotButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ItemImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ItemAmountText;

    // 对应你蓝图里的 Slot Image 2 (选中高亮框)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> SelectionHighlightImage;

private:
    // 缓存查到的静态数据，方便点击时直接广播出去，避免重复查询
    FItemData CachedItemData;
};