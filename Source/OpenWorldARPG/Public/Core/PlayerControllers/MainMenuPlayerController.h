// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PlayerControllers/OpenWorldARPGPlayerController.h"
#include "GameplayTagContainer.h"
#include "MainMenuPlayerController.generated.h"

class ULoginScreenWidget;

/**
 * 主菜单控制器 (M3)
 * 登录按钮不再直接进入开始界面：先经 GameServer 登录门禁，
 * 服务器确认成功后才流转到开始游戏界面，失败保留登录界面并反馈原因。
 */
UCLASS()
class OPENWORLDARPG_API AMainMenuPlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOnLoginButtonClicked();

    UFUNCTION()
    void HandleServerLoginResult(bool bSuccess, const FString& ErrorMsg);

    UFUNCTION()
    void HandleOnStartButtonClicked();

private:
    void AdvanceToStartScreen();

protected:
    // --- UI 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag BackgroundUITag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag LoginUITag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag StartGameUITag;

private:
    UPROPERTY()
    TObjectPtr<ULoginScreenWidget> LoginScreenWidget;
};
