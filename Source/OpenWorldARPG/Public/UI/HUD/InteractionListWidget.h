// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionListWidget.generated.h"

class UBorder;

/**
 * 交互列表容器 (对应 WBP_InteractionList)
 * 接收可交互 Actor 列表，管理条目生成与显示
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UInteractionListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|Interaction")
    void UpdateInteractionList(const TArray<AActor*>& InteractableActors);

protected:
    // --- 控件绑定 ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> Border_InteractiveList;
};