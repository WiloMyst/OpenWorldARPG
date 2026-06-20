// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "CloseGamePanelWidget.generated.h"

class UButton;

UCLASS()
class OPENWORLDARPG_API UCloseGamePanelWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Cancel;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Confirm;

private:
    UFUNCTION()
    void HandleCancelClicked();

    UFUNCTION()
    void HandleConfirmClicked();
};