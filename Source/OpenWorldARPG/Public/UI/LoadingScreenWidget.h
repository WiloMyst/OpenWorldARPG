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
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    UFUNCTION(BlueprintPure, Category = "Loading")
    float GetProgressBarPercent() const;

protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* LoadingProgressBar;

    UPROPERTY(BlueprintReadOnly, Category = "Loading")
    float LoadingProgress = 0.0f;

private:
    UPROPERTY()
    UGameAssetManagerSubsystem* AssetManagerSubsystem;
};