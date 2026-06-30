// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Screens/LoginScreenWidget.h"
#include "Components/Button.h"

void ULoginScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LoginButton)
    {
        LoginButton->OnClicked.AddDynamic(this, &ULoginScreenWidget::HandleLoginButtonClicked);
    }
}

void ULoginScreenWidget::HandleLoginButtonClicked()
{
    OnLoginButtonClicked.Broadcast();
}