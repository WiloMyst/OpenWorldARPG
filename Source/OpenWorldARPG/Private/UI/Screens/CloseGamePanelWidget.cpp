// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Screens/CloseGamePanelWidget.h"
#include "Components/Button.h"
#include "Managers/UIManagerSubsystem.h"
#include "Kismet/KismetSystemLibrary.h" // 包含退出游戏的静态库

void UCloseGamePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Cancel)
    {
        Button_Cancel->OnClicked.AddDynamic(this, &UCloseGamePanelWidget::HandleCancelClicked);
    }

    if (Button_Confirm)
    {
        Button_Confirm->OnClicked.AddDynamic(this, &UCloseGamePanelWidget::HandleConfirmClicked);
    }
}

void UCloseGamePanelWidget::HandleCancelClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->CloseTopUI();
        }
    }
}

void UCloseGamePanelWidget::HandleConfirmClicked()
{
    APlayerController* SpecificPlayer = GetOwningPlayer();
    UKismetSystemLibrary::QuitGame(
        this,
        SpecificPlayer,
        EQuitPreference::Quit,
        false // IgnorePlatformRestrictions
    );
}