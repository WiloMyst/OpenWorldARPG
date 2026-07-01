// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameplayListWidget.generated.h"

class UButton;

/**
 * 玩法列表菜单 (对应 WBP_GameplayList)
 * 右上角静态菜单入口（背包、角色面板等）
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGameplayListWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;

    // --- 控件绑定 ---
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_Inventory;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_Character;

    // --- 配置项（Tag 驱动，避免强耦合 Widget 类） ---

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag InventoryUITag;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag CharacterPanelUITag;

private:
    UFUNCTION()
    void OnInventoryButtonClicked();

    UFUNCTION()
    void OnCharacterButtonClicked();
};