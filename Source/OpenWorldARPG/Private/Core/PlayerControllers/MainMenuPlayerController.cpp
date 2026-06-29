// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainMenuPlayerController.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/GameFlowSubsystem.h"
#include "Core/GameModes/MainMenuGameMode.h"
#include "Data/StartingRosterConfig.h"
#include "UI/Screens/LoginScreenWidget.h"
#include "UI/Screens/StartGameScreenWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>();
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
    UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>();
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
    if (UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CloseTopUI();
    }

    // 2. 从 GameMode 取配置数据，交给 FlowManager 统筹切图
    // 注：主菜单作为纯本地关卡运行（Standalone / Listen Server Host），故可安全获取 AuthGameMode
    AMainMenuGameMode* MainMenuGM = Cast<AMainMenuGameMode>(UGameplayStatics::GetGameMode(this));
    UGameFlowSubsystem* FlowManager = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>();

    if (MainMenuGM && FlowManager)
    {
        UStartingRosterConfig* RosterConfig = MainMenuGM->GetStartingRosterConfig();
        TSoftObjectPtr<UWorld> TargetLevel = MainMenuGM->GetTargetLevelToLoad();
        FlowManager->RequestTravelFromMainMenu(TargetLevel, RosterConfig);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenuPlayerController: GameFlowSubsystem 或 MainMenuGameMode 获取失败！"));
    }
}