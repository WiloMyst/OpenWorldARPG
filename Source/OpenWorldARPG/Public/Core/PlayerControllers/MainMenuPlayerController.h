// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h" // 必须包含才能使用 FGameplayTag
#include "MainMenuPlayerController.generated.h"

UCLASS()
class OPENWORLDARPG_API AMainMenuPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    // 对应蓝图自定义事件：HandleOnLoginButtonClicked
    UFUNCTION()
    void HandleOnLoginButtonClicked();

    // 对应蓝图自定义事件：HandleOnStartButtonClicked
    UFUNCTION()
    void HandleOnStartButtonClicked();

protected:
    // ==========================================
    // UI 配置项 (请在蓝图细节面板中指定具体的 Tag)
    // ==========================================

    /**
     * @brief 登录界面的 Tag，通常为 UI.Menu.Login
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag LoginUITag;

    /**
     * @brief 开始游戏界面的 Tag，通常为 UI.Menu.StartGame
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag StartGameUITag;
};