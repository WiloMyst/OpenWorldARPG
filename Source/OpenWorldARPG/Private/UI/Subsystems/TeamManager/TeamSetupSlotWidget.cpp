// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/TeamManager/TeamSetupSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

UTeamSetupSlotWidget::UTeamSetupSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
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

FReply UTeamSetupSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.IsTouchEvent())
    {
        OnSlotClicked.Broadcast(CharacterTag, SlotIndex);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}
