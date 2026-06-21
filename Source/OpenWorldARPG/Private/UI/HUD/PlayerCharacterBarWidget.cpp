// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/PlayerCharacterBarWidget.h"
#include "Components/ProgressBar.h"

void UPlayerCharacterBarWidget::UpdateHealth(float CurrentHealth, float MaxHealth)
{
    if (HealthBar)
    {
        // 保护性计算：防止除以 0 导致浮点数异常
        float Percent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
        HealthBar->SetPercent(Percent);
    }
}