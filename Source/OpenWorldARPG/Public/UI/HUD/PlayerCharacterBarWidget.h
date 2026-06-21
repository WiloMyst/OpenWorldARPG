// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerCharacterBarWidget.generated.h"

class UProgressBar;

/**
 * 玩家角色状态条 UI (对应 WBP_PlayerCharacterBar)
 * 职责：独立管理并展示角色的血量、护盾、体力等基础状态表现
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UPlayerCharacterBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 更新血量表现 */
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void UpdateHealth(float CurrentHealth, float MaxHealth);

protected:
    // ==========================================
    // UI 组件绑定 (变量名必须与蓝图中的对应一致)
    // ==========================================
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UProgressBar> HealthBar;
};