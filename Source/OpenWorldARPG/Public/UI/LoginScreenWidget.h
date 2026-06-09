// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "LoginScreenWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoginButtonClicked);

UCLASS()
class OPENWORLDARPG_API ULoginScreenWidget : public UBaseMenuWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLoginButtonClicked OnLoginButtonClicked;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* LunchButton;

private:
    UFUNCTION()
    void HandleLoginButtonClicked();
};