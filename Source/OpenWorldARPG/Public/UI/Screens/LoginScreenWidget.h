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
 * 账号优先读取蓝图绑定输入框，缺省回退到项目设置开发账号；密码无缺省。
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

    // 密码输入：蓝图输入框优先（缺省无默认密码，判空由服务器拒绝）
    UFUNCTION(BlueprintPure, Category = "Login")
    FString GetPasswordInput() const;

    // --- 登录状态反馈（蓝图可选实现：禁用按钮 / 显示错误提示） ---

    UFUNCTION(BlueprintImplementableEvent, Category = "Login")
    void OnLoginPending();

    UFUNCTION(BlueprintImplementableEvent, Category = "Login")
    void OnLoginResult(bool bSuccess, const FString& Message);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* LoginButton;

    // 可选输入框：蓝图未提供时自动回退默认账号
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> AccountTextBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> PasswordTextBox;

private:
    UFUNCTION()
    void HandleLoginButtonClicked();
};
