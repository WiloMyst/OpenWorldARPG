// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Inventory/ItemCategoryTabWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"

void UItemCategoryTabWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (TabButton)
    {
        TabButton->OnClicked.RemoveDynamic(this, &UItemCategoryTabWidget::OnTabButtonClicked);
        TabButton->OnClicked.AddDynamic(this, &UItemCategoryTabWidget::OnTabButtonClicked);

        TabButton->OnHovered.RemoveDynamic(this, &UItemCategoryTabWidget::OnTabButtonHovered);
        TabButton->OnHovered.AddDynamic(this, &UItemCategoryTabWidget::OnTabButtonHovered);

        TabButton->OnUnhovered.RemoveDynamic(this, &UItemCategoryTabWidget::OnTabButtonUnhovered);
        TabButton->OnUnhovered.AddDynamic(this, &UItemCategoryTabWidget::OnTabButtonUnhovered);
    }

    if (ImageMouseHovered)
    {
        ImageMouseHovered->SetRenderOpacity(0.0f);
    }

    UpdateTabInfo();
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
            FStreamableDelegate Delegate;
            Delegate.BindUFunction(this, FName("OnCategoryIconLoaded"), CategoryTabData.CategoryIcon.ToSoftObjectPath());
            UAssetManager::GetStreamableManager().RequestAsyncLoad(CategoryTabData.CategoryIcon.ToSoftObjectPath(), Delegate);
        }
    }
}

void UItemCategoryTabWidget::OnCategoryIconLoaded(FSoftObjectPath LoadedPath)
{
    if (!CategoryIcon) return;

    if (UTexture2D* LoadedIcon = Cast<UTexture2D>(LoadedPath.ResolveObject()))
    {
        CategoryIcon->SetBrushFromTexture(LoadedIcon);
    }
}

void UItemCategoryTabWidget::SetTabSelectedState(bool bIsSelected)
{
    if (ImageMouseClicked) ImageMouseClicked->SetRenderOpacity(bIsSelected ? 1.0f : 0.0f);
}

void UItemCategoryTabWidget::OnTabButtonClicked()
{
    if (OnTabClicked.IsBound())
    {
        OnTabClicked.Broadcast(this, CategoryTabData.TabCategory);
    }
}

void UItemCategoryTabWidget::OnTabButtonHovered()
{
    if (ImageMouseHovered)
    {
        ImageMouseHovered->SetRenderOpacity(1.0f);
    }
}

void UItemCategoryTabWidget::OnTabButtonUnhovered()
{
    if (ImageMouseHovered)
    {
        ImageMouseHovered->SetRenderOpacity(0.0f);
    }
}
