// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/PlayerControlButtonWidget.h"
#include "Components/Button.h"
#include "Managers/UIManagerSubsystem.h"
#include "Engine/GameInstance.h"

void UPlayerControlButtonWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Button_OpenPlayerPanel)
    {
        Button_OpenPlayerPanel->OnClicked.AddDynamic(this, &UPlayerControlButtonWidget::OnPlayerPanelButtonClicked);
    }
}

void UPlayerControlButtonWidget::OnPlayerPanelButtonClicked()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->ShowUIByTag(PlayerPanelUITag);
        }
    }
}