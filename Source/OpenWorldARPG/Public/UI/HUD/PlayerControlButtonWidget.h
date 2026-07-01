// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PlayerControlButtonWidget.generated.h"

class UButton;

/**
 * 玩家控制面板呼出按钮 (对应 WBP_PlayerControlButton)
 * 点击调用 UIManager 打开玩家界面
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UPlayerControlButtonWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;

    // --- 控件绑定 ---
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_OpenPlayerPanel;

    // --- 配置项 ---

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag PlayerPanelUITag;

private:
    UFUNCTION()
    void OnPlayerPanelButtonClicked();
};