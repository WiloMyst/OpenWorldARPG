// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseMenuWidget.generated.h"

/** Widget 打开时的输入模式。 */
UENUM(BlueprintType)
enum class EWidgetInputMode : uint8
{
    UIOnly,
    GameAndUI,
    GameOnly,
    UIOnlyNoCursor
};

UCLASS()
class OPENWORLDARPG_API UBaseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, DisplayName = "打开时的输入模式")
    EWidgetInputMode InputModeWhenOpen = EWidgetInputMode::UIOnly;

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "BaseMenuWidget")
    void OnOpened();

    UFUNCTION(BlueprintImplementableEvent, Category = "BaseMenuWidget")
    void OnClosed();
};