// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameplayListWidget.generated.h"

class UButton;

/**
 * 玩法列表菜单控件 (对应 WBP_GameplayList)
 * 职责：管理右上角的静态菜单入口（如背包、角色面板等）
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGameplayListWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    // 使用 NativeOnInitialized 进行一次性事件绑定，比 NativeConstruct 更高效
    virtual void NativeOnInitialized() override;

    // ==========================================
    // UI 组件绑定 (变量名必须与蓝图中的对应一致)
    // ==========================================
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_Inventory;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UButton> Button_Character;

    // ==========================================
    // 配置项 (通过 Tag 驱动 UI 打开，避免强耦合具体 Widget 类)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag InventoryUITag;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag CharacterPanelUITag;

private:
    // --- 按钮点击回调 ---
    
    UFUNCTION()
    void OnInventoryButtonClicked();

    UFUNCTION()
    void OnCharacterButtonClicked();
};