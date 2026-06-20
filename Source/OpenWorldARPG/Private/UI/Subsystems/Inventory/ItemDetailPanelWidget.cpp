// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/Inventory/ItemDetailPanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Managers/InventoryManagerSubsystem.h"

void UItemDetailPanelWidget::UpdateDetails(const FItemInstance& ItemInstance)
{
    UInventoryManagerSubsystem* InventoryManager = GetGameInstance()->GetSubsystem<UInventoryManagerSubsystem>();
    if (!InventoryManager) return;

    FItemData StaticData;
    if (!InventoryManager->GetItemStaticData(ItemInstance.ItemID, StaticData))
    {
        // 查询失败，清空面板
        if (NameText) NameText->SetText(FText::GetEmpty());
        if (AvatarImage) AvatarImage->SetRenderOpacity(0.0f);
        if (CategoryText) CategoryText->SetText(FText::GetEmpty());
        if (AmountText) AmountText->SetText(FText::GetEmpty());
        if (FunctionText) FunctionText->SetText(FText::GetEmpty());
        if (DetailText) DetailText->SetText(FText::GetEmpty());
        if (SourceText) SourceText->SetText(FText::GetEmpty());
        return;
    }

    // --- 赋值：物品名称 ---
    if (NameText)
    {
        NameText->SetText(StaticData.ItemName);
    }

    // --- 赋值：物品图标 (异步加载) ---
    if (AvatarImage)
    {
        if (!StaticData.ItemIcon.IsNull())
        {
            if (UTexture2D* LoadedIcon = StaticData.ItemIcon.Get())
            {
                // 已加载：直接使用
                AvatarImage->SetBrushFromTexture(LoadedIcon);
                AvatarImage->SetRenderOpacity(1.0f);
            }
            else
            {
                // 未加载：异步请求，加载完成后由 StreamableManager 回调
                FStreamableDelegate Delegate;
                Delegate.BindUFunction(this, FName("OnDetailIconLoaded"), StaticData.ItemIcon.ToSoftObjectPath());
                UAssetManager::GetStreamableManager().RequestAsyncLoad(StaticData.ItemIcon.ToSoftObjectPath(), Delegate);
            }
        }
        else
        {
            AvatarImage->SetRenderOpacity(0.0f);
        }
    }

    // --- 赋值：物品分类 ---
    if (CategoryText)
    {
        if (const UEnum* EnumPtr = StaticEnum<EItemCategory>())
        {
            FText CategoryTextName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(StaticData.ItemCategory));
            CategoryText->SetText(CategoryTextName);
        }
    }

    // --- 赋值：拥有数量 ---
    if (AmountText)
    {
        FText AmountFormat = NSLOCTEXT("InventoryUI", "ItemAmountText", "拥有 {0}");
        FText FinalAmountText = FText::Format(AmountFormat, FText::AsNumber(ItemInstance.Count));
        AmountText->SetText(FinalAmountText);
    }

    // --- 赋值：功能描述 ---
    if (FunctionText)
    {
        FunctionText->SetText(StaticData.ItemFunctionDescription);
    }

    // --- 赋值：详细描述 ---
    if (DetailText)
    {
        DetailText->SetText(StaticData.ItemDescription);
    }

    // --- 赋值：来源说明 ---
    if (SourceText)
    {
        // TODO: 如果 StaticData 包含来源字段，在此赋值
    }
}

void UItemDetailPanelWidget::OnDetailIconLoaded(FSoftObjectPath LoadedPath)
{
    if (!AvatarImage) return;

    if (UTexture2D* LoadedIcon = Cast<UTexture2D>(LoadedPath.ResolveObject()))
    {
        AvatarImage->SetBrushFromTexture(LoadedIcon);
        AvatarImage->SetRenderOpacity(1.0f);
    }
}
