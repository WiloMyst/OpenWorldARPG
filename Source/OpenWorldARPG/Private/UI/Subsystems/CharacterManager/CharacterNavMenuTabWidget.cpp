// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/CharacterManager/CharacterNavMenuTabWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

UCharacterNavMenuTabWidget::UCharacterNavMenuTabWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterNavMenuTabWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // 编辑器预览：将 DefaultTabName 同步到文本控件
    if (TabNameText)
    {
        TabNameText->SetText(DefaultTabName);
    }
}

void UCharacterNavMenuTabWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定按钮点击事件
    if (TabButton)
    {
        TabButton->OnClicked.AddDynamic(this, &UCharacterNavMenuTabWidget::HandleTabButtonClicked);
    }
}

void UCharacterNavMenuTabWidget::SetTabName(const FText& InName)
{
    DefaultTabName = InName;

    if (TabNameText)
    {
        TabNameText->SetText(InName);
    }
}

void UCharacterNavMenuTabWidget::SetTabId(const FGameplayTag& InTabId)
{
    TabId = InTabId;
}

void UCharacterNavMenuTabWidget::HandleTabButtonClicked()
{
    // 向父容器广播：我被点击了，附带自己的 TabId
    OnTabClickedDelegate.Broadcast(TabId);
}
