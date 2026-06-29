// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/CharacterManager/CharacterNavMenuWidget.h"

UCharacterNavMenuWidget::UCharacterNavMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterNavMenuWidget::SelectNav(int32 NavIndex)
{
    if (NavIndex == SelectedNavIndex) return;

    SelectedNavIndex = NavIndex;

    // 通知蓝图更新视觉
    OnNavSelectionChanged(NavIndex);

    // 向上传递事件
    OnNavChanged.Broadcast(NavIndex);
}
