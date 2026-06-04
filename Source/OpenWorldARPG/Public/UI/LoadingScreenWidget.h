// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

class UProgressBar;
class UGameAssetManagerSubsystem;

UCLASS()
class OPENWORLDARPG_API ULoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    // 对应蓝图的 Event Construct (用于缓存变量，避免在 Tick 中高频调用 GetSubsystem)
    virtual void NativeConstruct() override;

    // 对应蓝图的 Event Tick
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    // 对应你截图2中的纯函数。
    // 注意：优化后我们其实不再需要它，但为了严格对齐你的设计，这里予以保留。
    UFUNCTION(BlueprintPure, Category = "Loading")
    float GetProgressBarPercent() const;

protected:
    // 核心优化：直接捕获 UI 设计器里的进度条组件
    // 变量名必须与你在 UMG 编辑器里给进度条起的名字完全一致！
    UPROPERTY(meta = (BindWidget))
    UProgressBar* LoadingProgressBar;

    // 对应蓝图的 Loading Progress 变量
    UPROPERTY(BlueprintReadOnly, Category = "Loading")
    float LoadingProgress = 0.0f;

private:
    // 缓存资产管理子系统指针，极致压榨性能
    UPROPERTY()
    UGameAssetManagerSubsystem* AssetManagerSubsystem;
};