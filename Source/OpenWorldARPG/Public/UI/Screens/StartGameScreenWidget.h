// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "StartGameScreenWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartButtonClicked);

UCLASS()
class OPENWORLDARPG_API UStartGameScreenWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStartButtonClicked OnStartButtonClicked;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* StartButton;

private:
    UFUNCTION()
    void HandleStartButtonClicked();
};