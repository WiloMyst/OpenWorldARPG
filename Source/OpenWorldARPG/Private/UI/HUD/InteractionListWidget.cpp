// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/InteractionListWidget.h"
#include "Components/Border.h"

void UInteractionListWidget::UpdateInteractionList(const TArray<AActor*>& InteractableActors)
{
    // 目前保留原有的显示/隐藏逻辑，后续可以在此扩展 ScrollBox 动态生成条目的逻辑
    if (Border_InteractiveList)
    {
        Border_InteractiveList->SetRenderOpacity(InteractableActors.Num() > 0 ? 1.0f : 0.0f);
    }
}