// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "StartGameScreenWidget.generated.h"

class UButton;

// 声明我们在上一步 PlayerController 中绑定过的委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartButtonClicked);

UCLASS()
class OPENWORLDARPG_API UStartGameScreenWidget : public UBaseMenuWidget
{
    GENERATED_BODY()

public:
    // 对应蓝图中的事件派发器 "On Start Button Clicked"
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStartButtonClicked OnStartButtonClicked;

protected:
    virtual void NativeConstruct() override;

    // 对应蓝图中的 StartButton
    UPROPERTY(meta = (BindWidget))
    UButton* StartButton;

private:
    UFUNCTION()
    void HandleStartButtonClicked();
};