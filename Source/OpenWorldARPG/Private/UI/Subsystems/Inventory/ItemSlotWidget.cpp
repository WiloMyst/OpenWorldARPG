// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/ItemSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Managers/InventoryManagerSubsystem.h"

void UItemSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.RemoveDynamic(this, &UItemSlotWidget::OnSlotButtonClicked);
        SlotButton->OnClicked.AddDynamic(this, &UItemSlotWidget::OnSlotButtonClicked);
    }

    UpdateSlotInfo();
}

void UItemSlotWidget::UpdateSlotInfo()
{
    if (!ItemImage || !ItemAmountText) return;

    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (!InventoryManager) return;

    if (InventoryManager->GetItemStaticData(ItemInstance.ItemID, CachedItemData))
    {
        if (!CachedItemData.ItemIcon.IsNull())
        {
            // 优先尝试直接获取已加载的资源
            if (UTexture2D* LoadedIcon = CachedItemData.ItemIcon.Get())
            {
                ItemImage->SetBrushFromTexture(LoadedIcon);
                ItemImage->SetRenderOpacity(1.0f);
            }
            else
            {
                // 未加载：异步请求，加载完成后回调 OnSlotIconLoaded
                FStreamableDelegate Delegate;
                Delegate.BindUFunction(this, FName("OnSlotIconLoaded"), CachedItemData.ItemIcon.ToSoftObjectPath());
                UAssetManager::GetStreamableManager().RequestAsyncLoad(CachedItemData.ItemIcon.ToSoftObjectPath(), Delegate);
                // 加载期间先隐藏图标
                ItemImage->SetRenderOpacity(0.0f);
            }
        }
        else
        {
            ItemImage->SetRenderOpacity(0.0f);
        }
    }
    else
    {
        ItemImage->SetRenderOpacity(0.0f);
    }

    ItemAmountText->SetText(FText::AsNumber(ItemInstance.Count));
    ItemAmountText->SetRenderOpacity(ItemInstance.Count > 1 ? 1.0f : 0.0f);
}

void UItemSlotWidget::OnSlotIconLoaded(FSoftObjectPath LoadedPath)
{
    if (!ItemImage) return;

    if (UTexture2D* LoadedIcon = Cast<UTexture2D>(LoadedPath.ResolveObject()))
    {
        ItemImage->SetBrushFromTexture(LoadedIcon);
        ItemImage->SetRenderOpacity(1.0f);
    }
}

void UItemSlotWidget::OnSlotButtonClicked()
{
    if (OnSlotClicked.IsBound())
    {
        OnSlotClicked.Broadcast(ItemArrayIndex, ItemInstance, CachedItemData, this);
    }
}

void UItemSlotWidget::SetSelectionState(bool bIsSelected)
{
    if (SelectionHighlightImage)
    {
        SelectionHighlightImage->SetRenderOpacity(bIsSelected ? 1.0f : 0.0f);
    }
}
