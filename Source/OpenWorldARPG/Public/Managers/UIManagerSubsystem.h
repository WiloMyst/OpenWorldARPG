// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "UIManagerSubsystem.generated.h"

class UWindowWidgetBase;
class UUIDataAsset;

/**
 * UI 管理子系统。基于栈的 UI 打开/关闭管理，自动处理输入模式切换。
 */
UCLASS()
class OPENWORLDARPG_API UUIManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** 通过 Tag 显示 UI（从 UIDataAsset 查找对应 WidgetClass） */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    UWindowWidgetBase* ShowUIByTag(FGameplayTag UITag);

    /** 打开指定 WidgetClass 的 UI */
    UWindowWidgetBase* OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass);

    /** 关闭栈顶 UI */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseTopUI();

    /** 是否有 UI 处于打开状态 */
    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsAnyUIOpen() const;

    /** 根据 UI 设置更新 PlayerController 的输入模式 */
    void UpdateInputMode();

protected:
    /** UI 栈，后进先出 */
    UPROPERTY()
    TArray<TObjectPtr<UWindowWidgetBase>> UIStack;
};