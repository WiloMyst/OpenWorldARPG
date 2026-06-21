// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionListWidget.generated.h"

class UBorder;

/**
 * 交互列表容器 UI (对应 WBP_InteractionList)
 * 职责：接收可交互的 Actor 列表，管理内部的滚动框和交互条目的生成与显示。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UInteractionListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 供外部 (主 HUD) 调用的数据刷新接口
    UFUNCTION(BlueprintCallable, Category = "UI|Interaction")
    void UpdateInteractionList(const TArray<AActor*>& InteractableActors);

protected:
    // ==========================================
    // UI 组件绑定 (变量名必须与蓝图中完全一致)
    // ==========================================
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> Border_InteractiveList;

    // 未来如果要用 C++ 动态生成条目，可以在这里把滚动框也绑进来：
    // UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    // TObjectPtr<UScrollBox> ScrollBox_List;
};