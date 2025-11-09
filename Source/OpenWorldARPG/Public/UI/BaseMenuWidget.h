// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseMenuWidget.generated.h"

// 描述当Widget被打开时，应用的输入模式
UENUM(BlueprintType)
enum class EWidgetInputMode : uint8
{
    // 游戏暂停，所有输入给UI
    UIOnly,
    // 游戏继续，输入同时给游戏和UI
    GameAndUI,
    // 游戏继续，输入只给游戏
    GameOnly,
	// 游戏暂停，所有输入给UI，无光标
    UIOnlyNoCursor
};

UCLASS()
class OPENWORLDARPG_API UBaseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // --- 在蓝图的类默认值中设置这些属性 ---

    // 当这个Widget被打开时，应用的输入模式
    UPROPERTY(EditDefaultsOnly, DisplayName = "打开时的输入模式")
    EWidgetInputMode InputModeWhenOpen = EWidgetInputMode::UIOnly;

public:
    // --- 可以在蓝图中覆写的事件 ---

    // 当这个Widget被UIManager显示时调用
    UFUNCTION(BlueprintImplementableEvent, Category = "BaseMenuWidget")
    void OnOpened();

    // 当这个Widget被UIManager关闭时调用
    UFUNCTION(BlueprintImplementableEvent, Category = "BaseMenuWidget")
    void OnClosed();
};