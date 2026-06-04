// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "PlayerPanelWidget.generated.h"

class UButton;

UCLASS()
class OPENWORLDARPG_API UPlayerPanelWidget : public UBaseMenuWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    // 绑定蓝图中的关闭按钮，变量名必须一模一样
    UPROPERTY(meta = (BindWidget))
    UButton* Button_ClosePlayerPanel;

private:
    UFUNCTION()
    void HandleCloseClicked();
};