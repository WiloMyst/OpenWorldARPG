// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/UI/CharacterCarouselItemWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

UCharacterCarouselItemWidget::UCharacterCarouselItemWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterCarouselItemWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定按钮点击事件（大厂规范：使用 UButton 而非 NativeOnMouseButtonDown）
    if (ItemButton)
    {
        ItemButton->OnClicked.AddUniqueDynamic(this, &UCharacterCarouselItemWidget::OnItemButtonClicked);
    }
}

void UCharacterCarouselItemWidget::InitializeItem(const FGameplayTag& InCharacterTag, UTexture2D* InHeadIcon)
{
    CharacterTag = InCharacterTag;

    if (HeadIconImage && InHeadIcon)
    {
        HeadIconImage->SetBrushFromTexture(InHeadIcon);
    }
}

void UCharacterCarouselItemWidget::OnItemButtonClicked()
{
    OnItemSelected.Broadcast(CharacterTag);
}
