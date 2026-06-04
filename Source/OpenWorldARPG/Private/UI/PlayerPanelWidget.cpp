// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/PlayerPanelWidget.h"
#include "Components/Button.h"
#include "Managers/UIManagerSubsystem.h"

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
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }
}