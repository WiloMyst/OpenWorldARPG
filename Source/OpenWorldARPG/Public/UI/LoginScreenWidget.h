// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BaseMenuWidget.h"
#include "LoginScreenWidget.generated.h"

class UButton;

// 声明我们在上一步 PlayerController 中绑定过的委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoginButtonClicked);

UCLASS()
class OPENWORLDARPG_API ULoginScreenWidget : public UBaseMenuWidget
{
    GENERATED_BODY()

public:
    // 对应蓝图中的事件派发器 "On Lunch Button Clicked"
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLoginButtonClicked OnLoginButtonClicked;

protected:
    // 覆盖父类的 NativeConstruct，相当于蓝图的 Event Construct
    virtual void NativeConstruct() override;

    // 绑定蓝图中的按钮。变量名必须与蓝图层级面板中的名称完全一致！
    UPROPERTY(meta = (BindWidget))
    UButton* LunchButton;

private:
    // 用于响应按钮点击的内部 C++ 函数
    UFUNCTION()
    void HandleLoginButtonClicked();
};