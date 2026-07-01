// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "UIManagerSubsystem.generated.h"

class UWindowWidgetBase;
class UUIDataAsset;
class ULoadingScreenWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIStackChangedSignature);

/**
 * UI 管理子系统。基于栈的 UI 打开/关闭管理，自动处理输入模式切换。
 * 继承 LocalPlayerSubsystem：每玩家独立 UI 栈，天然支持本地多人分屏。
 */
UCLASS()
class OPENWORLDARPG_API UUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    // --- UI 栈管理 ---

    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    UWindowWidgetBase* ShowUIByTag(FGameplayTag UITag);

    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    UWindowWidgetBase* OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseTopUI();

    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseUIByTag(const FGameplayTag& UITag);

    // --- 状态查询 ---

    bool IsAnyUIOpen() const { return !UIStack.IsEmpty(); }
    bool IsUIOpen(const FGameplayTag& UITag) const;
    UWindowWidgetBase* GetTopWindowWidget() const { return UIStack.IsEmpty() ? nullptr : UIStack.Last(); }

    // --- 加载界面 ---

    void ShowLoadingScreen();
    void HideLoadingScreen();

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "UI Manager")
    FOnUIStackChangedSignature OnUIStackChanged;

protected:
    // --- 运行时状态 ---

    UPROPERTY()
    TArray<TObjectPtr<UWindowWidgetBase>> UIStack;

    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UWindowWidgetBase>> ActiveUIs;

    UPROPERTY()
    TObjectPtr<ULoadingScreenWidget> LoadingScreenInstance;
};
