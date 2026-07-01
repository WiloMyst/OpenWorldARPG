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
 * 继承 LocalPlayerSubsystem：每玩家独立 UI 栈，天然支持本地多人分屏，
 * 且与 PlayerController/LocalPlayer 绑定的输入模式切换更直接。
 */
UCLASS()
class OPENWORLDARPG_API UUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 通过 Tag 显示 UI（从 UIDataAsset 查找 WidgetClass） */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    UWindowWidgetBase* ShowUIByTag(FGameplayTag UITag);

    UWindowWidgetBase* OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass);

    /** 关闭栈顶 UI */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseTopUI();

    /** 通过 Tag 关闭指定 UI（即使在栈中也可精确关闭） */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseUIByTag(const FGameplayTag& UITag);

    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsAnyUIOpen() const;

    /** UI 生命周期唯一事实来源 (SSOT) 的状态查询 */
    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsUIOpen(const FGameplayTag& UITag) const;

	UFUNCTION(BlueprintPure, Category = "UI Manager")
	UWindowWidgetBase* GetTopWindowWidget() const;

    UPROPERTY(BlueprintAssignable, Category = "UI Manager")
    FOnUIStackChangedSignature OnUIStackChanged;

    // --- 加载界面管理 ---

    /** 显示加载界面（Z-Order=100，独立于 UIStack） */
    UFUNCTION(BlueprintCallable, Category = "UI Manager|Loading")
    void ShowLoadingScreen();

    UFUNCTION(BlueprintCallable, Category = "UI Manager|Loading")
    void HideLoadingScreen();

protected:
    UPROPERTY()
    TArray<TObjectPtr<UWindowWidgetBase>> UIStack;

    /** Tag → Widget 映射，与 UIStack 并行维护，提供 O(1) 查询 */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UWindowWidgetBase>> ActiveUIs;

    UPROPERTY()
    TObjectPtr<ULoadingScreenWidget> LoadingScreenInstance;
};