// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/LoginScreenWidget.h"
#include "Components/Button.h" // 必须包含 Button 组件头文件

void ULoginScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 在构造时，将 C++ 函数绑定到按钮的 OnClicked 事件上
    if (LunchButton)
    {
        LunchButton->OnClicked.AddDynamic(this, &ULoginScreenWidget::HandleLoginButtonClicked);
    }
}

void ULoginScreenWidget::HandleLoginButtonClicked()
{
    // 对应蓝图的 "调用 On Lunch Button Clicked" 节点
    OnLoginButtonClicked.Broadcast();
}