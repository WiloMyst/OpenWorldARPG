// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/GameFlowSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Data/StartingRosterConfig.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

void UGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CurrentGameState = EGameState::Idle;
}

// ####################################################################
// #                         流程入口                                   #
// ####################################################################

void UGameFlowSubsystem::RequestTravelFromMainMenu(TSoftObjectPtr<UWorld> TargetLevel, UStartingRosterConfig* RosterConfig)
{
    if (CurrentGameState != EGameState::Idle)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameFlow: 已有切图流程进行中，忽略新请求。"));
        return;
    }

    if (TargetLevel.IsNull() || !RosterConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("GameFlow: TargetLevel 或 RosterConfig 无效！"));
        return;
    }

    PendingTargetLevel = TargetLevel;

    // 阶段 1：初始化队伍数据
    InitializeTeamData(RosterConfig);

    // 阶段 2：UI Block — 弹出 Loading 屏
    BeginUIBlock();

    // 阶段 3：Asset Block — 启动资源加载
    BeginAssetBlock();
}

// ####################################################################
// #                         管线各阶段                                 #
// ####################################################################

void UGameFlowSubsystem::InitializeTeamData(UStartingRosterConfig* RosterConfig)
{
    UGameInstance* GI = GetGameInstance();

    UCharacterManagerSubsystem* CharManager = GI->GetSubsystem<UCharacterManagerSubsystem>();
    UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>();

    if (CharManager && TeamManager)
    {
        CharManager->InitializeFromDataObject(RosterConfig);
        TeamManager->InitializeFromDataObject(RosterConfig);
    }
}

void UGameFlowSubsystem::BeginUIBlock()
{
    CurrentGameState = EGameState::Traveling;

    // 通知 UIManager 弹出 Loading 屏（屏蔽输入由 UI 栈自动处理）
    if (UUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->ShowLoadingScreen();
    }
}

void UGameFlowSubsystem::BeginAssetBlock()
{
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    if (!AssetManager) return;

    // 绑定关卡预加载完成事件（移除旧绑定防止重复）
    AssetManager->OnLevelAsyncLoaded.RemoveDynamic(this, &UGameFlowSubsystem::OnLevelPreloadFinished);
    AssetManager->OnLevelAsyncLoaded.AddDynamic(this, &UGameFlowSubsystem::OnLevelPreloadFinished);

    // 绑定所有资源加载完成事件
    AssetManager->OnLoadComplete.RemoveDynamic(this, &UGameFlowSubsystem::OnAllAssetsLoaded);
    AssetManager->OnLoadComplete.AddDynamic(this, &UGameFlowSubsystem::OnAllAssetsLoaded);

    // 启动关卡异步预加载（不再传 Loading 屏逻辑给 AssetManager）
    AssetManager->TryStartAsyncLevelLoading(PendingTargetLevel);
}

// ####################################################################
// #                         回调                                       #
// ####################################################################

void UGameFlowSubsystem::OnLevelPreloadFinished()
{
    // 关卡预加载完成 → 启动队伍资源加载
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>();

    if (AssetManager && TeamManager)
    {
        TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();
        AssetManager->StartTeamAssetLoading(TeamTags);
        UE_LOG(LogTemp, Log, TEXT("GameFlow: 关卡预加载完成，已启动队伍资产加载。"));
    }
}

void UGameFlowSubsystem::OnAllAssetsLoaded()
{
    UE_LOG(LogTemp, Log, TEXT("GameFlow: 所有资产加载完成，执行切图。"));
    ExecuteTravel();
}

void UGameFlowSubsystem::ExecuteTravel()
{
    // 阶段 4：执行底层切图
    FName LevelName = FName(*FPackageName::GetShortName(PendingTargetLevel.GetLongPackageName()));
    UGameplayStatics::OpenLevel(GetWorld(), LevelName);

    // 注意：OpenLevel 后当前 World 会被销毁，后续收尾在新关卡的 NotifyNewLevelReady 中完成
}

// ####################################################################
// #                         新关卡报到                                 #
// ####################################################################

void UGameFlowSubsystem::NotifyNewLevelReady()
{
    if (CurrentGameState != EGameState::Traveling)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameFlow: 收到 NewLevelReady 但当前不在 Traveling 状态。"));
        return;
    }

    // 阶段 5：收尾 — 触发资源清理 + 关闭 Loading 屏
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    if (AssetManager)
    {
        AssetManager->CleanupAfterLoad();
    }

    if (UUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->HideLoadingScreen();
    }

    CurrentGameState = EGameState::Playing;
    UE_LOG(LogTemp, Log, TEXT("GameFlow: 新关卡就绪，切图流程完成。"));
}

// ####################################################################
// #                         辅助函数                                   #
// ####################################################################

UUIManagerSubsystem* UGameFlowSubsystem::GetUIManager() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (APlayerController* PC = GI->GetFirstLocalPlayerController())
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                return LP->GetSubsystem<UUIManagerSubsystem>();
            }
        }
    }
    return nullptr;
}
