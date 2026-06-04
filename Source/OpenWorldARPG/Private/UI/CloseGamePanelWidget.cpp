// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/CloseGamePanelWidget.h"
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
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }
}

void UCloseGamePanelWidget::HandleConfirmClicked()
{
    // 对应蓝图的 "退出游戏 (Quit Game)" 节点
    APlayerController* SpecificPlayer = GetOwningPlayer();
    UKismetSystemLibrary::QuitGame(
        this,
        SpecificPlayer,
        EQuitPreference::Quit,
        false // IgnorePlatformRestrictions
    );
}