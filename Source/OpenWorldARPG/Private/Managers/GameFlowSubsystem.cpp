// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/GameFlowSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Data/InitialArchiveData.h"
#include "Core/OpenWorldARPGSettings.h"
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

void UGameFlowSubsystem::RequestTravelFromMainMenu(TSoftObjectPtr<UWorld> TargetLevel)
{
    if (CurrentGameState != EGameState::Idle)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameFlow: 已有切图流程进行中，忽略新请求。"));
        return;
    }

    if (TargetLevel.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("GameFlow: TargetLevel 无效！"));
        return;
    }

    PendingTargetLevel = TargetLevel;

    // 阶段 1：UI Block — 弹出 Loading 屏
    BeginUIBlock();

    // 阶段 2：初始化队伍数据（从 UOpenWorldARPGSettings 加载 UInitialArchiveData）
    InitializeTeamData();

    // 阶段 3：Asset Block — 启动资源加载
    BeginAssetBlock();
}

// ####################################################################
// #                         管线各阶段                                 #
// ####################################################################

void UGameFlowSubsystem::InitializeTeamData()
{
    // 从项目设置加载初始存档数据（UInitialArchiveData）
    const TSoftObjectPtr<UInitialArchiveData>& ArchiveSoftPtr = UOpenWorldARPGSettings::Get().InitialArchiveData;
    if (ArchiveSoftPtr.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("GameFlow: OpenWorldARPGSettings.InitialArchiveData 未配置！无法初始化队伍数据。"));
        return;
    }

    UInitialArchiveData* ArchiveData = ArchiveSoftPtr.LoadSynchronous();
    if (!ArchiveData)
    {
        UE_LOG(LogTemp, Error, TEXT("GameFlow: InitialArchiveData 同步加载失败！"));
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    for (int32 i = 0; i < GI->GetNumLocalPlayers(); ++i)
    {
        if (ULocalPlayer* LocalPlayer = GI->GetLocalPlayerByIndex(i))
        {
            UCharacterManagerSubsystem* CharManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
            UTeamManagerSubsystem* TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>();

            if (CharManager)
            {
                CharManager->InitializeFromDataObject(ArchiveData);
            }
            if (TeamManager)
            {
                TeamManager->InitializeFromDataObject(ArchiveData);
            }
        }
    }
}

void UGameFlowSubsystem::BeginUIBlock()
{
    CurrentGameState = EGameState::Traveling;

    // 通知 UIManager 弹出 Loading 屏（屏蔽输入由 UI 栈自动处理）
    if (UUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->ShowLoadingScreen();
        while (UIManager->IsAnyUIOpen())
        {
            UIManager->CloseTopUI();
        }
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
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    if (!AssetManager) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    for (int32 i = 0; i < GI->GetNumLocalPlayers(); ++i)
    {
        if (ULocalPlayer* LocalPlayer = GI->GetLocalPlayerByIndex(i))
        {
            if (UTeamManagerSubsystem* TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
            {
                TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();
                AssetManager->StartTeamAssetLoading(TeamTags);
                UE_LOG(LogTemp, Log, TEXT("GameFlow: 关卡预加载完成，已启动队伍资产加载。"));
                break;
            }
        }
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
