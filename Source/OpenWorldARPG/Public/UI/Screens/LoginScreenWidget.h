// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Core/WindowWidgetBase.h"
#include "LoginScreenWidget.generated.h"

class UButton;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoginButtonClicked);

/**
 * 登录界面 (M3)
 * 按钮点击仅广播事件，服务器登录与界面流转由 AMainMenuPlayerController 统一驱动。
 * 账号/令牌优先读取蓝图绑定的输入框，缺省回退到项目设置的开发账号。
 */
UCLASS()
class OPENWORLDARPG_API ULoginScreenWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLoginButtonClicked OnLoginButtonClicked;

    // --- 登录凭据 ---

    // 账号输入：蓝图输入框优先，空则回退项目设置默认账号
    UFUNCTION(BlueprintPure, Category = "Login")
    FString GetAccountInput() const;

    // 令牌输入：蓝图输入框优先，空则回退项目设置静态令牌
    UFUNCTION(BlueprintPure, Category = "Login")
    FString GetTokenInput() const;

    // --- 登录状态反馈（蓝图可选实现：禁用按钮 / 显示错误提示） ---

    UFUNCTION(BlueprintImplementableEvent, Category = "Login")
    void OnLoginPending();

    UFUNCTION(BlueprintImplementableEvent, Category = "Login")
    void OnLoginResult(bool bSuccess, const FString& Message);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* LoginButton;

    // 可选输入框：蓝图未提供时自动回退默认凭据
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> AccountTextBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> TokenTextBox;

private:
    UFUNCTION()
    void HandleLoginButtonClicked();
};
