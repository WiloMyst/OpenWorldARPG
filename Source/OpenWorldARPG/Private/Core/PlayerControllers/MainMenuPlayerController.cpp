// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainMenuPlayerController.h"
#include "UI/Core/UIManagerSubsystem.h"
#include "UI/Screens/LoginScreenWidget.h"
#include "UI/Screens/StartGameScreenWidget.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "Systems/GameFlowManager/GameFlowSubsystem.h"
#include "Core/GameModes/MainMenuGameMode.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager) return;

    // 1. 先显示持久化的背景 UI（底层）
    if (BackgroundUITag.IsValid())
    {
        UIManager->ShowUIByTag(BackgroundUITag);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuPlayerController: BackgroundUITag 未配置，将没有背景图。"));
    }

    // 2. 安全检查：确保蓝图里配置了 LoginUITag
    if (!LoginUITag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: LoginUITag 未配置！请检查蓝图。"));
        return;
    }

    // 3. 显示登录交互面板（上层，覆盖在背景之上）
    UWindowWidgetBase* LoginWidgetBase = UIManager->ShowUIByTag(LoginUITag);

    // 4. 类型转换为具体的登录UI类，并绑定事件
    LoginScreenWidget = Cast<ULoginScreenWidget>(LoginWidgetBase);
    if (LoginScreenWidget)
    {
        // 先移除可能残留的绑定（安全起见），然后添加新绑定
        LoginScreenWidget->OnLoginButtonClicked.RemoveDynamic(this, &AMainMenuPlayerController::HandleOnLoginButtonClicked);
        LoginScreenWidget->OnLoginButtonClicked.AddDynamic(this, &AMainMenuPlayerController::HandleOnLoginButtonClicked);
    }
}

void AMainMenuPlayerController::HandleOnLoginButtonClicked()
{
    UGameServerSubsystem* GameServer = GetGameInstance()->GetSubsystem<UGameServerSubsystem>();
    if (!GameServer || !LoginScreenWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: GameServerSubsystem 或登录界面缺失，跳过服务器登录门禁。"));
        AdvanceToStartScreen();
        return;
    }

    // 已登录（断线重连场景）直接放行
    if (GameServer->IsLoggedIn())
    {
        AdvanceToStartScreen();
        return;
    }

    // 防重复点击：登录请求进行中忽略
    if (GameServer->IsLoginPending())
    {
        return;
    }

    GameServer->OnLoginResult.RemoveDynamic(this, &AMainMenuPlayerController::HandleServerLoginResult);
    GameServer->OnLoginResult.AddDynamic(this, &AMainMenuPlayerController::HandleServerLoginResult);

    LoginScreenWidget->OnLoginPending();
    GameServer->RequestLogin(LoginScreenWidget->GetAccountInput(), LoginScreenWidget->GetTokenInput());
    UE_LOG(LogTemp, Log, TEXT("MainMenuPlayerController: 已发起服务器登录，等待确认。"));
}

void AMainMenuPlayerController::HandleServerLoginResult(bool bSuccess, const FString& ErrorMsg)
{
    if (UGameServerSubsystem* GameServer = GetGameInstance()->GetSubsystem<UGameServerSubsystem>())
    {
        GameServer->OnLoginResult.RemoveDynamic(this, &AMainMenuPlayerController::HandleServerLoginResult);
    }

    if (!bSuccess)
    {
        // 登录失败：保留登录界面并反馈原因
        if (LoginScreenWidget)
        {
            LoginScreenWidget->OnLoginResult(false, ErrorMsg.IsEmpty() ? TEXT("登录失败") : ErrorMsg);
        }
        UE_LOG(LogTemp, Warning, TEXT("MainMenuPlayerController: 服务器登录失败: %s"), *ErrorMsg);
        return;
    }

    if (LoginScreenWidget)
    {
        LoginScreenWidget->OnLoginResult(true, TEXT(""));
    }
    AdvanceToStartScreen();
}

void AMainMenuPlayerController::AdvanceToStartScreen()
{
    UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager) return;

    // 1. 关闭登录交互面板。
    //    注意：此时不会黑屏闪烁，因为底层持久的 BackgroundUI 仍在渲染全屏背景原画。
    UIManager->CloseTopUI();

    // 2. 安全检查：确保蓝图里配置了 StartGameUITag
    if (!StartGameUITag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: StartGameUITag 未配置！请检查蓝图。"));
        return;
    }

    // 3. 显示开始游戏交互面板（上层，覆盖在背景之上）
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
    if (UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }

    // 2. 从 GameMode 取目标关卡，交给 FlowManager 统筹切图。
    //    初始存档数据（UInitialArchiveData）由 GameFlowSubsystem 从 UOpenWorldARPGSettings 自行加载，
    //    PlayerController / GameMode 均不再持有。
    //    注：主菜单作为纯本地关卡运行（Standalone / Listen Server Host），故可安全获取 AuthGameMode
    AMainMenuGameMode* MainMenuGM = Cast<AMainMenuGameMode>(UGameplayStatics::GetGameMode(this));
    UGameFlowSubsystem* FlowManager = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();

    if (MainMenuGM && FlowManager)
    {
        TSoftObjectPtr<UWorld> TargetLevel = MainMenuGM->GetTargetLevelToLoad();
        FlowManager->RequestTravelFromMainMenu(TargetLevel);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: GameFlowSubsystem 或 MainMenuGameMode 获取失败！"));
    }
}
