// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/GameplayListWidget.h"
#include "Components/Button.h"
#include "Managers/UIManagerSubsystem.h"
#include "Engine/GameInstance.h"

void UGameplayListWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 绑定背包按钮点击事件
    if (Button_Inventory)
    {
        Button_Inventory->OnClicked.AddDynamic(this, &UGameplayListWidget::OnInventoryButtonClicked);
    }

    // 绑定角色面板按钮点击事件
    if (Button_Character)
    {
        Button_Character->OnClicked.AddDynamic(this, &UGameplayListWidget::OnCharacterButtonClicked);
    }
}

void UGameplayListWidget::OnInventoryButtonClicked()
{
    // 呼叫全局 UI 管理器打开背包
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->ShowUIByTag(InventoryUITag);
        }
    }
}

void UGameplayListWidget::OnCharacterButtonClicked()
{
    // 呼叫全局 UI 管理器打开角色面板
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->ShowUIByTag(CharacterPanelUITag);
        }
    }
}