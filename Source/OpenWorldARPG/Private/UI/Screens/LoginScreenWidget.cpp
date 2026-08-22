// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Screens/LoginScreenWidget.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

void ULoginScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LoginButton)
    {
        LoginButton->OnClicked.AddDynamic(this, &ULoginScreenWidget::HandleLoginButtonClicked);
    }
}

FString ULoginScreenWidget::GetAccountInput() const
{
    if (AccountTextBox)
    {
        const FString Input = AccountTextBox->GetText().ToString().TrimStartAndEnd();
        if (!Input.IsEmpty())
        {
            return Input;
        }
    }
    return UOpenWorldARPGSettings::Get().GameServerDefaultAccount;
}

FString ULoginScreenWidget::GetTokenInput() const
{
    if (TokenTextBox)
    {
        const FString Input = TokenTextBox->GetText().ToString().TrimStartAndEnd();
        if (!Input.IsEmpty())
        {
            return Input;
        }
    }
    return UOpenWorldARPGSettings::Get().GameServerStaticToken;
}

void ULoginScreenWidget::HandleLoginButtonClicked()
{
    OnLoginButtonClicked.Broadcast();
}
