// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainMenuPlayerController.h"
#include "Managers/UIManagerSubsystem.h"
#include "Core/GameModes/MainMenuGameMode.h"
#include "UI/Screens/LoginScreenWidget.h"
#include "UI/Screens/StartGameScreenWidget.h"
#include "Kismet/GameplayStatics.h"

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager) return;

    // 1. 安全检查：确保蓝图里配置了 Tag
    if (!LoginUITag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: LoginUITag 未配置！请检查蓝图。"));
        return;
    }

    // 2. 显示登录界面 (直接使用配置的 Tag)
    UWindowWidgetBase* LoginWidgetBase = UIManager->ShowUIByTag(LoginUITag);

    // 3. 类型转换为具体的登录UI类，并绑定事件
    if (ULoginScreenWidget* LoginWidget = Cast<ULoginScreenWidget>(LoginWidgetBase))
    {
        // 先移除可能残留的绑定（安全起见），然后添加新绑定
        LoginWidget->OnLoginButtonClicked.RemoveDynamic(this, &AMainMenuPlayerController::HandleOnLoginButtonClicked);
        LoginWidget->OnLoginButtonClicked.AddDynamic(this, &AMainMenuPlayerController::HandleOnLoginButtonClicked);
    }
}

void AMainMenuPlayerController::HandleOnLoginButtonClicked()
{
    UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager) return;

    // 1. 关闭登录界面
    UIManager->CloseTopUI();

    // 2. 安全检查：确保蓝图里配置了 StartGameUITag
    if (!StartGameUITag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: StartGameUITag 未配置！请检查蓝图。"));
        return;
    }

    // 3. 显示开始游戏界面 (直接使用配置的 Tag)
    UWindowWidgetBase* StartWidgetBase = UIManager->ShowUIByTag(StartGameUITag);

    // 4. 类型转换为具体的开始UI类，并绑定事件
    if (UStartGameScreenWidget* StartWidget = Cast<UStartGameScreenWidget>(StartWidgetBase))
    {
        StartWidget->OnStartButtonClicked.RemoveDynamic(this, &AMainMenuPlayerController::HandleOnStartButtonClicked);
        StartWidget->OnStartButtonClicked.AddDynamic(this, &AMainMenuPlayerController::HandleOnStartButtonClicked);
    }
}

void AMainMenuPlayerController::HandleOnStartButtonClicked()
{
    // 1. 关闭开始游戏界面
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }

    // TODO [联机架构缺陷]: GetGameMode 在客户端返回 nullptr。
    // 联机时客户端点击"开始游戏"需要通过 Server RPC 通知服务器。
    // 当前仅在 Host/单机场景下有效。
    if (AMainMenuGameMode* MainMenuGM = Cast<AMainMenuGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        MainMenuGM->HandleStartGameRequest();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: 无法获取 MainMenuGameMode！"));
    }
}