// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/PlayerManager/UI/PlayerPanelWidget.h"
#include "Components/Button.h"
#include "UI/Core/UIManagerSubsystem.h"

void UPlayerPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_ClosePlayerPanel)
    {
        Button_ClosePlayerPanel->OnClicked.AddDynamic(this, &UPlayerPanelWidget::HandleCloseClicked);
    }
}

void UPlayerPanelWidget::HandleCloseClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->CloseTopUI();
        }
    }
}