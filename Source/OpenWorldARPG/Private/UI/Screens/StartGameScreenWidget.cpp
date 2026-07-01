// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Screens/StartGameScreenWidget.h"
#include "Components/Button.h"

void UStartGameScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (StartButton)
    {
        StartButton->OnClicked.AddDynamic(this, &UStartGameScreenWidget::HandleStartButtonClicked);
    }
}

void UStartGameScreenWidget::HandleStartButtonClicked()
{
    OnStartButtonClicked.Broadcast();
}