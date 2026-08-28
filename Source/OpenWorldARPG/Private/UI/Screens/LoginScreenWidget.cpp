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

FString ULoginScreenWidget::GetPasswordInput() const
{
    if (PasswordTextBox)
    {
        const FString Input = PasswordTextBox->GetText().ToString();
        if (!Input.IsEmpty())
        {
            return Input;
        }
    }
    return FString();
}

void ULoginScreenWidget::HandleLoginButtonClicked()
{
    OnLoginButtonClicked.Broadcast();
}
