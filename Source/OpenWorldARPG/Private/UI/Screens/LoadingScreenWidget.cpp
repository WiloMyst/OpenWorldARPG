// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Screens/LoadingScreenWidget.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Components/ProgressBar.h"
#include "Engine/GameInstance.h"

void ULoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 在 UI 创建时，就把 Subsystem 找出来存好，不要在 Tick 里每帧去找
    if (UGameInstance* GI = GetGameInstance())
    {
        AssetManagerSubsystem = GI->GetSubsystem<UGameAssetManagerSubsystem>();
    }
}

void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (AssetManagerSubsystem)
    {
        LoadingProgress = AssetManagerSubsystem->GetTotalLoadingProgress();

        // 直接设置进度条的值，无需 UMG 绑定
        if (LoadingProgressBar)
        {
            LoadingProgressBar->SetPercent(LoadingProgress);
        }
    }
}

float ULoadingScreenWidget::GetProgressBarPercent() const
{
    // 对应截图2：返回当前的 Loading Progress
    return LoadingProgress;
}