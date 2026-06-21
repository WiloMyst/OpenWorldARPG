// Copyright 2025 WiloMyst. All Rights Reserved.


#include "UI/Core/WindowWidgetBase.h"

UWindowWidgetBase::UWindowWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 允许该弹窗类 Widget 接收键盘/鼠标焦点
    // 消除 Attempting to focus Non-Focusable widget 报错，并解决需要点两下才能激活的 Bug
    bIsFocusable = true;
}