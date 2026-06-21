// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PlayerControlButtonWidget.generated.h"

class UButton;

/**
 * 玩家控制面板呼出按钮 UI (对应 WBP_PlayerControlButton)
 * 职责：独立管理点击事件，呼叫全局 UIManager 打开玩家界面
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UPlayerControlButtonWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;

    // ==========================================
    // 控件绑定 (变量名必须与蓝图中完全一致)
    // ==========================================
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_OpenPlayerPanel;

    // ==========================================
    // 配置项
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag PlayerPanelUITag;

private:
    UFUNCTION()
    void OnPlayerPanelButtonClicked();
};