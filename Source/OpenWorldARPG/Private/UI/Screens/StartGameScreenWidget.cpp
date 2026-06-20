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
    // 对应蓝图的 "调用 On Start Button Clicked" 节点
    OnStartButtonClicked.Broadcast();
}