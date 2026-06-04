// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainMenuGameMode.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Data/StartingRosterConfig.h"
#include "Engine/GameInstance.h"

void AMainMenuGameMode::HandleStartGameRequest()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    // 1. 获取所有需要的子系统
    UCharacterManagerSubsystem* CharManager = GI->GetSubsystem<UCharacterManagerSubsystem>();
    UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>();
    UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>();

    if (!CharManager || !TeamManager || !AssetManager)
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenu: 子系统获取失败！"));
        return;
    }

    // 2. 读取初始配置数据 (对应蓝图里的 In Data Object 引脚)
    UStartingRosterConfig* ConfigData = StartingRosterAsset.LoadSynchronous();
    if (!ConfigData)
    {
        UE_LOG(LogTemp, Error, TEXT("MainMenu: 未配置初始玩家数据！"));
        return;
    }

    // 3. 执行初始化逻辑 (对应蓝图序列 Then 1)
    CharManager->InitializeFromDataObject(ConfigData);
    TeamManager->InitializeFromDataObject(ConfigData);

    // 4. 绑定关卡加载完成的事件 (对应蓝图里的 Bind Event to On Level Async Loaded)
    // 注意：在绑定前最好先 RemoveDynamic 防止重复绑定
    AssetManager->OnLevelAsyncLoaded.RemoveDynamic(this, &AMainMenuGameMode::OnLevelPreloadFinished);
    AssetManager->OnLevelAsyncLoaded.AddDynamic(this, &AMainMenuGameMode::OnLevelPreloadFinished);

    // 5. 开始加载关卡
    AssetManager->TryStartAsyncLevelLoading(TargetLevelToLoad);
}

void AMainMenuGameMode::OnLevelPreloadFinished()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>();
    UGameAssetManagerSubsystem* AssetManager = GI->GetSubsystem<UGameAssetManagerSubsystem>();

    if (TeamManager && AssetManager)
    {
        // 获取当前队伍并开始加载资产 (对应蓝图回调事件后的逻辑)
        TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();
        AssetManager->StartTeamAssetLoading(TeamTags);

        UE_LOG(LogTemp, Log, TEXT("MainMenu: 关卡加载完成，已启动队伍资产加载流程。"));
    }
}