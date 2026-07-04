// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InventoryManager/UI/ItemSlotWidget.h"
#include "Systems/InventoryManager/UI/ItemObject.h"
#include "Systems/InventoryManager/UI/ItemSlotPanelWidget.h"
#include "Systems/InventoryManager/UI/InventoryViewModel.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UItemSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 按钮事件绑定（仅一次，避免重复绑定）
    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UItemSlotWidget::OnSlotButtonClicked);
    }
}

void UItemSlotWidget::NativeDestruct()
{
    // 取消未完成的异步加载，防止界面销毁后回调野指针崩溃
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    // 解绑旧 UItemObject 的选中委托 (防泄漏)
    if (BoundItemObject)
    {
        BoundItemObject->OnSelectionStateChanged.RemoveDynamic(this, &UItemSlotWidget::OnItemSelectionStateChanged);
        BoundItemObject->OnItemDataChanged.RemoveDynamic(this, &UItemSlotWidget::UpdateSlotInfo);
        BoundItemObject = nullptr;
    }

    Super::NativeDestruct();
}

void UItemSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    // 解绑旧 UItemObject 的所有委托 (防重复绑定导致崩溃)
    if (BoundItemObject)
    {
        BoundItemObject->OnSelectionStateChanged.RemoveDynamic(this, &UItemSlotWidget::OnItemSelectionStateChanged);
        BoundItemObject->OnItemDataChanged.RemoveDynamic(this, &UItemSlotWidget::UpdateSlotInfo);
    }

    UItemObject* ItemObj = Cast<UItemObject>(ListItemObject);
    BoundItemObject = ItemObj;

    if (ItemObj)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] Slot Widget 绑定了新的 ItemObject, ItemID: %d"), ItemObj->ItemInstance.ItemID);

        // 绑定新 UItemObject 的选中状态变化委托
        ItemObj->OnSelectionStateChanged.AddDynamic(this, &UItemSlotWidget::OnItemSelectionStateChanged);
        // 绑定新 UItemObject 的物品数据变化委托
        ItemObj->OnItemDataChanged.AddDynamic(this, &UItemSlotWidget::UpdateSlotInfo);

        // 应用当前选中状态
        SetSelectionState(ItemObj->bIsSelected);

        UpdateSlotInfo();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] Slot Widget 绑定的 ListItemObject 为空或强转失败！"));
    }
}

void UItemSlotWidget::UpdateSlotInfo()
{
    if (!ItemImage || !ItemAmountText || !BoundItemObject) return;

    const FItemInstance& ItemInstance = BoundItemObject->ItemInstance;

    // 直接通过 UItemObject 缓存的静态数据指针访问配置 (禁止 UI 内部调用 DataTable 查询)
    const FItemData* StaticData = BoundItemObject->CachedStaticData;
    if (!StaticData) return;

    // 只要开始刷新槽位，先掐断之前残留的异步加载任务
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    if (!StaticData->ItemIcon.IsNull())
    {
        // 优先尝试直接获取已加载的资源
        if (UTexture2D* LoadedIcon = StaticData->ItemIcon.Get())
        {
            ItemImage->SetBrushFromTexture(LoadedIcon);
            ItemImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            // 未加载：异步请求，使用强类型绑定并缓存 Handle
            TSoftObjectPtr<UTexture2D> SoftIcon = StaticData->ItemIcon;
            FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UItemSlotWidget::OnSlotIconLoaded, SoftIcon);
            IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftIcon.ToSoftObjectPath(), Delegate);

            // 加载期间先隐藏图标
            ItemImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        ItemImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 数量文本：仅大于 1 时显示
    ItemAmountText->SetText(FText::AsNumber(ItemInstance.Count));
    ItemAmountText->SetVisibility(ItemInstance.Count > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UItemSlotWidget::OnSlotIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (!ItemImage) return;

    if (SoftIcon.IsValid())
    {
        ItemImage->SetBrushFromTexture(SoftIcon.Get());
        ItemImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

void UItemSlotWidget::OnSlotButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] ======= 槽位按钮被点击 ======="));

    if (!BoundItemObject)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] 点击失败: BoundItemObject 为空！"));
        return;
    }

    // --- 纯净版：使用你原本的 GetOuter() 逻辑，绝不包含被废弃的 ParentViewModel ---
    UObject* OuterObj = BoundItemObject->GetOuter();
    if (!OuterObj)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] 点击失败: BoundItemObject->GetOuter() 为空！"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] BoundItemObject 的 Outer 类型是: %s"), *OuterObj->GetClass()->GetName());

    UInventoryViewModel* VM = Cast<UInventoryViewModel>(OuterObj);
    if (!VM)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] 点击失败: Outer 无法强转为 UInventoryViewModel！由于 TileView 的复用机制，Outer 极有可能发生了改变。"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] 成功拿到 ViewModel，准备通知选中 ItemID: %d"), BoundItemObject->ItemInstance.ItemID);
    VM->SelectItem(BoundItemObject);
}

void UItemSlotWidget::OnItemSelectionStateChanged(UItemObject* ItemObject, bool bIsSelected)
{
    SetSelectionState(bIsSelected);
}

void UItemSlotWidget::SetSelectionState(bool bIsSelected)
{
    if (SelectionHighlightImage)
    {
        SelectionHighlightImage->SetVisibility(bIsSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}
