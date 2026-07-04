// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InventoryManager/UI/ItemCategoryTabWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UItemCategoryTabWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 按钮事件绑定（仅一次，避免重复绑定）
    if (TabButton)
    {
        TabButton->OnClicked.AddDynamic(this, &UItemCategoryTabWidget::OnTabButtonClicked);
    }
}

void UItemCategoryTabWidget::NativeDestruct()
{
    // 取消未完成的异步加载，防止界面销毁后回调野指针崩溃
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    Super::NativeDestruct();
}

void UItemCategoryTabWidget::UpdateTabInfo()
{
    if (CategoryNameText)
    {
        CategoryNameText->SetText(CategoryTabData.CategoryNameText);
    }

    if (CategoryIcon && !CategoryTabData.CategoryIcon.IsNull())
    {
        if (UTexture2D* LoadedIcon = CategoryTabData.CategoryIcon.Get())
        {
            CategoryIcon->SetBrushFromTexture(LoadedIcon);
        }
        else
        {
            // 如果有旧的加载任务，先取消
            if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
            {
                IconLoadHandle->CancelHandle();
            }

            // 强类型绑定并缓存 Handle
            TSoftObjectPtr<UTexture2D> SoftIcon = CategoryTabData.CategoryIcon;
            FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UItemCategoryTabWidget::OnCategoryIconLoaded, SoftIcon);
            IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftIcon.ToSoftObjectPath(), Delegate);
        }
    }
}

void UItemCategoryTabWidget::OnCategoryIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (!CategoryIcon) return;

    if (SoftIcon.IsValid())
    {
        CategoryIcon->SetBrushFromTexture(SoftIcon.Get());
    }
}

void UItemCategoryTabWidget::SetSelected(bool bIsSelected)
{
    if (TabButton)
    {
        TabButton->SetIsEnabled(!bIsSelected);
    }
}

void UItemCategoryTabWidget::OnTabButtonClicked()
{
    if (OnTabClicked.IsBound())
    {
        OnTabClicked.Broadcast(this, CategoryTabData.TabCategory);
    }
}