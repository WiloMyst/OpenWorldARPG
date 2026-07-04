// Copyright 2025 WilloMyst. All Rights Reserved.

#include "Systems/TeamManager/UI/TeamSetupSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

UTeamSetupSlotWidget::UTeamSetupSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UTeamSetupSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ItemButton)
    {
        ItemButton->OnClicked.AddUniqueDynamic(this, &UTeamSetupSlotWidget::OnButtonClicked);
    }
}

void UTeamSetupSlotWidget::InitializeSlot(const FGameplayTag& InCharacterTag, const FText& InDisplayName, UTexture2D* InHeadIcon, int32 InSlotIndex)
{
    CharacterTag = InCharacterTag;
    SlotIndex = InSlotIndex;

    if (NameText)
    {
        NameText->SetText(InDisplayName);
    }

    if (HeadIconImage && InHeadIcon)
    {
        HeadIconImage->SetBrushFromTexture(InHeadIcon);
    }
}

void UTeamSetupSlotWidget::OnButtonClicked()
{
    // 通过 UButton 原生事件触发委托，避免事件吞噬
    OnSlotClicked.Broadcast(CharacterTag, SlotIndex);
}

void UTeamSetupSlotWidget::SetHighlighted(bool bIsHighlighted)
{
    // 纯 C++ 视觉反馈：已上阵/选中 → 绿色，否则 → 白色
    if (ItemButton)
    {
        const FLinearColor Color = bIsHighlighted
            ? FLinearColor(0.8f, 1.0f, 0.2f, 1.0f)
            : FLinearColor::White;
        ItemButton->SetBackgroundColor(Color);
    }
}
