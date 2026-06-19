// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "Data/UIDataAsset.h"
#include "UI/LoadingScreenWidget.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemGlobals.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"

void UGameAssetManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 强制 GAS 初始化全局数据并读取 DefaultGame.ini 里的自定义配置
    UAbilitySystemGlobals::Get().InitGlobalData();

    UAssetManager& AssetManager = UAssetManager::Get();
    StreamableManager = &AssetManager.GetStreamableManager();
}

// --- 中央资产缓存访问接口 ---

UDataTable* UGameAssetManagerSubsystem::GetCharacterInfoTable()
{
    if (!CachedCharacterInfoTable)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedCharacterInfoTable = Settings.CharacterInfoTable.LoadSynchronous();
        if (!CachedCharacterInfoTable)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: CharacterInfoTable 未配置或加载失败！请在项目设置中检查。"));
        }
    }
    return CachedCharacterInfoTable;
}

UDataTable* UGameAssetManagerSubsystem::GetItemDatabaseTable()
{
    if (!CachedItemDatabaseTable)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedItemDatabaseTable = Settings.ItemDatabaseTable.LoadSynchronous();
        if (!CachedItemDatabaseTable)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: ItemDatabaseTable 未配置或加载失败！请在项目设置中检查。"));
        }
    }
    return CachedItemDatabaseTable;
}

UDataTable* UGameAssetManagerSubsystem::GetInventoryCategoryTabDataTable()
{
    if (!CachedInventoryCategoryTabDataTable)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedInventoryCategoryTabDataTable = Settings.InventoryCategoryTabDataTable.LoadSynchronous();
        if (!CachedInventoryCategoryTabDataTable)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: InventoryCategoryTabDataTable 未配置或加载失败！请在项目设置中检查。"));
        }
    }
    return CachedInventoryCategoryTabDataTable;
}

UCharacterGeneralDataAsset* UGameAssetManagerSubsystem::GetPlayerCharacterGeneralAbilityDataAsset()
{
    if (!CachedPlayerCharacterGeneralAbilityDataAsset)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedPlayerCharacterGeneralAbilityDataAsset = Settings.PlayerCharacterGeneralAbilityDataAsset.LoadSynchronous();
        if (!CachedPlayerCharacterGeneralAbilityDataAsset)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: PlayerCharacterGeneralAbilityDataAsset 未配置或加载失败！请在项目设置中检查。"));
        }
    }
    return CachedPlayerCharacterGeneralAbilityDataAsset;
}

UUIDataAsset* UGameAssetManagerSubsystem::GetUIMapDataAsset()
{
    if (!CachedUIMapDataAsset)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedUIMapDataAsset = Settings.UIMapDataAsset.LoadSynchronous();
        if (!CachedUIMapDataAsset)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: UIMapDataAsset 未配置或加载失败！请在项目设置中检查。"));
        }
    }
    return CachedUIMapDataAsset;
}

TSubclassOf<ULoadingScreenWidget> UGameAssetManagerSubsystem::GetLoadingScreenWidgetClass() const
{
    const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
    return Settings.LoadingScreenWidgetClass;
}

// --- 异步加载调度 ---

float UGameAssetManagerSubsystem::GetTotalLoadingProgress() const
{
    float Progress = 0.0f;

    switch (CurrentLoadingPhase)
    {
    case ELoadingPhase::LoadingLevel:
    {
        float LevelPhaseProgress = 0.0f;
        if (LevelLoadHandle.IsValid())
        {
            LevelPhaseProgress = LevelLoadHandle->GetProgress();
        }
        Progress = LevelPhaseProgress * LevelLoadWeight;
        break;
    }
    case ELoadingPhase::LoadingTeamAssets:
    {
        // 两阶段加载：阶段 2a（数据资产）+ 阶段 2b（内部资源）
        float AssetPhaseProgress = 0.0f;

        // 阶段 2a：数据资产加载进度
        if (DataAssetLoadHandle.IsValid())
        {
            AssetPhaseProgress += DataAssetLoadHandle->GetProgress() * DataAssetPhaseWeight;
        }
        else
        {
            // 2a 已完成，计入满进度
            AssetPhaseProgress += DataAssetPhaseWeight;
        }

        // 阶段 2b：内部资源加载进度
        if (!TeamAssetLoadHandles.IsEmpty())
        {
            float AssetProgressSum = 0.0f;
            for (const auto& Handle : TeamAssetLoadHandles)
            {
                if (Handle.IsValid())
                {
                    AssetProgressSum += Handle->GetProgress();
                }
            }
            AssetPhaseProgress += (AssetProgressSum / TeamAssetLoadHandles.Num()) * InnerAssetPhaseWeight;
        }

        Progress = LevelLoadWeight + (AssetPhaseProgress * TeamAssetLoadWeight);
        break;
    }
    case ELoadingPhase::Complete:
    {
        Progress = 1.0f;
        break;
    }
    case ELoadingPhase::None:
    default:
        Progress = 0.0f;
        break;
    }

    return Progress;
}

void UGameAssetManagerSubsystem::TryStartAsyncLevelLoading(TSoftObjectPtr<UWorld> LevelToLoad)
{
    if (CurrentLoadingPhase != ELoadingPhase::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadLevel: Already in a loading process."));
        return;
    }
    if (LevelToLoad.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("LoadLevel: LevelToLoad is not valid."));
        return;
    }

    CurrentLevelToLoad = LevelToLoad;
    TargetLevelName = FName(*FPackageName::GetShortName(LevelToLoad.GetLongPackageName()));

    ShowLoadingScreen();
    StartLevelLoading();
}

// ####################################################################
// #                         阶段一：关卡加载                           #
// ####################################################################

void UGameAssetManagerSubsystem::StartLevelLoading()
{
    CurrentLoadingPhase = ELoadingPhase::LoadingLevel;

    LevelLoadHandle = StreamableManager->RequestAsyncLoad(
        CurrentLevelToLoad.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UGameAssetManagerSubsystem::OnLevelLoadCompleted)
    );

    if (!LevelLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start async load for level: %s"), *TargetLevelName.ToString());
        OnLevelLoadCompleted(); // 即使失败也要继续流程
    }
}

void UGameAssetManagerSubsystem::OnLevelLoadCompleted()
{
    UE_LOG(LogTemp, Log, TEXT("Phase 1 Complete: Level preload finished."));
    OnLevelAsyncLoaded.Broadcast();
}

// ####################################################################
// #                      阶段二：队伍资产加载                          #
// ####################################################################

void UGameAssetManagerSubsystem::StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags)
{
    CurrentLoadingPhase = ELoadingPhase::LoadingTeamAssets;
    TeamAssetLoadHandles.Empty();
    CompletedAssetLoads = 0;
    FailedAssetLoads = 0;

    CurrentTeamToLoad = TeamCharacterTags;

    // ==========================================
    // 两阶段加载：阶段 2a — 异步加载 VisualDataAsset / CombatDataAsset 本身
    // ==========================================
    // FCharacterRegistryRow 中的 VisualData / CombatData 是 TSoftObjectPtr，
    // 直接调用 Get() 会触发同步加载（阻塞主线程）。
    // 必须先将它们异步加载完成，才能在阶段 2b 中安全访问内部字段（AbilityMontages 等）。
    StartDataAssetLoading();
}

void UGameAssetManagerSubsystem::StartDataAssetLoading()
{
    TArray<FSoftObjectPath> DataAssetPaths;

    UDataTable* LoadedTable = GetCharacterInfoTable();
    UCharacterManagerSubsystem* CharManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    if (LoadedTable && CharManager)
    {
        for (const FGameplayTag& CharTag : CurrentTeamToLoad)
        {
            FName RowName = CharManager->GetRowNameByTag(CharTag);
            if (RowName == NAME_None) continue;

            const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
            if (Row)
            {
                // 阶段 2a：收集 VisualDataAsset 和 CombatDataAsset 的路径
                // 这些是 DataAsset 本身，加载后才能安全访问其内部的 TSoftObjectPtr 字段
                DataAssetPaths.Add(Row->VisualData.ToSoftObjectPath());
                DataAssetPaths.Add(Row->CombatData.ToSoftObjectPath());
            }
        }
    }

    if (DataAssetPaths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Phase 2a: No data assets to load, skipping to completion."));
        OnAllTeamAssetsLoaded();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Phase 2a: Loading %d data assets (VisualDataAsset/CombatDataAsset)..."), DataAssetPaths.Num());

    // 批量异步加载所有数据资产，加载完成后进入阶段 2b
    DataAssetLoadHandle = StreamableManager->RequestAsyncLoad(
        DataAssetPaths,
        FStreamableDelegate::CreateUObject(this, &UGameAssetManagerSubsystem::OnDataAssetsLoaded)
    );

    if (!DataAssetLoadHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Phase 2a: Failed to start data asset loading!"));
        OnAllTeamAssetsLoaded();
    }
}

void UGameAssetManagerSubsystem::OnDataAssetsLoaded()
{
    UE_LOG(LogTemp, Log, TEXT("Phase 2a Complete: Data assets loaded. Starting phase 2b (inner assets)..."));

    // 数据资产已加载完成，释放 2a 句柄（资产已被 2b 的引用链持有）
    // 注意：不能立即释放，因为 2b 还需要访问这些资产的字段
    // 将 2a 句柄保留到 TeamAssetLoadHandles 中，随 2b 一起分帧释放
    if (DataAssetLoadHandle.IsValid())
    {
        TeamAssetLoadHandles.Add(DataAssetLoadHandle);
        DataAssetLoadHandle.Reset();
    }

    StartInnerAssetLoading();
}

void UGameAssetManagerSubsystem::StartInnerAssetLoading()
{
    // ==========================================
    // 两阶段加载：阶段 2b — 异步加载所有具体资源
    // ==========================================
    // 此时 VisualDataAsset / CombatDataAsset 已在内存中，
    // 可以安全遍历其内部字段收集所有 TSoftObjectPtr 路径。
    TArray<FSoftObjectPath> AssetPathsToLoad;

    UDataTable* LoadedTable = GetCharacterInfoTable();
    UCharacterManagerSubsystem* CharManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    if (LoadedTable && CharManager)
    {
        for (const FGameplayTag& CharTag : CurrentTeamToLoad)
        {
            FName RowName = CharManager->GetRowNameByTag(CharTag);
            if (RowName == NAME_None) continue;

            const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
            if (Row)
            {
                // ==========================================
                // UI 资产：从 FCharacterRegistryRow (DataTable) 加载
                // SSOT 原则：UI 展示数据只在此处配置
                // ==========================================
                AssetPathsToLoad.Add(Row->HeadIcon.ToSoftObjectPath());
                AssetPathsToLoad.Add(Row->SplashArt.ToSoftObjectPath());

                // ==========================================
                // 外观资产：从 UCharacterVisualDataAsset 加载
                // 三层解耦：VisualData 通过 TSoftObjectPtr 桥梁引用
                // 包含技能蒙太奇字典（AbilityMontages）的预加载
                // ==========================================
                if (UCharacterVisualDataAsset* VisualData = Row->VisualData.Get())
                {
                    // 骨骼网格体
                    AssetPathsToLoad.Add(VisualData->CharacterMesh.ToSoftObjectPath());

                    // 动画蓝图（TSoftClassPtr 的路径也是 FSoftObjectPath）
                    AssetPathsToLoad.Add(VisualData->AnimationBlueprint.ToSoftObjectPath());

                    // 动画层
                    AssetPathsToLoad.Add(VisualData->AimAnimLayers.ToSoftObjectPath());
                    AssetPathsToLoad.Add(VisualData->UpperBodyLayers.ToSoftObjectPath());
                    AssetPathsToLoad.Add(VisualData->PhysicsAnimLayers.ToSoftObjectPath());

                    // 武器蓝图
                    AssetPathsToLoad.Add(VisualData->WeaponBlueprint.ToSoftObjectPath());

                    // 非战斗蒙太奇（攀爬、钩索等，直接在 VisualDataAsset 中配置）
                    AssetPathsToLoad.Add(VisualData->ClimbUpMontage.ToSoftObjectPath());
                    AssetPathsToLoad.Add(VisualData->GrappleMontage.ToSoftObjectPath());
                }

                // ==========================================
                // 战斗资产：从 UCharacterCombatDataAsset 加载
                // 三层解耦：CombatData 通过 TSoftObjectPtr 桥梁引用
                // 连招蒙太奇直接在 ComboGraph 节点中配置
                // ==========================================
                if (UCharacterCombatDataAsset* CombatData = Row->CombatData.Get())
                {
                    for (const auto& TalentPair : CombatData->CharacterTalents)
                    {
                        const FTalentConfig& Talent = TalentPair.Value;
                        AssetPathsToLoad.Add(Talent.Icon.ToSoftObjectPath());

                        // 预加载连招图中的蒙太奇
                        for (const auto& NodePair : Talent.ComboGraph)
                        {
                            AssetPathsToLoad.Add(NodePair.Value.Montage.ToSoftObjectPath());
                        }
                    }
                }
            }
        }
    }

    TeamAssetLoadNum = AssetPathsToLoad.Num();
    if (TeamAssetLoadNum == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Phase 2b: No inner assets to load."));
        OnAllTeamAssetsLoaded();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Phase 2b: Loading %d inner assets (Mesh/Montage/Icon/AnimBP)..."), TeamAssetLoadNum);

    for (const FSoftObjectPath& Path : AssetPathsToLoad)
    {
        if (Path.IsValid())
        {
            TSharedPtr<FStreamableHandle> Handle = StreamableManager->RequestAsyncLoad(
                Path,
                FStreamableDelegate::CreateUObject(this, &UGameAssetManagerSubsystem::OnSingleTeamAssetLoaded)
            );
            TeamAssetLoadHandles.Add(Handle);
        }
        else
        {
            FailedAssetLoads++;
        }
    }

    // 如果有无效路径，检查是否已经满足完成条件
    if (CompletedAssetLoads + FailedAssetLoads >= TeamAssetLoadNum)
    {
        OnAllTeamAssetsLoaded();
    }
}

void UGameAssetManagerSubsystem::OnSingleTeamAssetLoaded()
{
    CompletedAssetLoads++;
    if (CompletedAssetLoads + FailedAssetLoads >= TeamAssetLoadNum)
    {
        OnAllTeamAssetsLoaded();
    }
}

void UGameAssetManagerSubsystem::OnAllTeamAssetsLoaded()
{
    // 注意：不在此时清空 TeamAssetLoadHandles！
    // 句柄保持持有引用，直到 CleanupAfterLoad 分帧释放，防止资产在 OpenLevel 前被 GC 回收
    TeamAssetLoadNum = 0;
    CurrentLoadingPhase = ELoadingPhase::Complete;
    UE_LOG(LogTemp, Log, TEXT("Phase 2 Complete: All team assets loaded."));

    UGameplayStatics::OpenLevel(GetWorld(), TargetLevelName);
}

// ####################################################################
// #                         UI 和清理辅助函数                          #
// ####################################################################

void UGameAssetManagerSubsystem::ShowLoadingScreen()
{
    if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

    TSubclassOf<ULoadingScreenWidget> WidgetClass = GetLoadingScreenWidgetClass();
    if (WidgetClass && !LoadingScreenInstance)
    {
        LoadingScreenInstance = CreateWidget<ULoadingScreenWidget>(GetGameInstance(), WidgetClass);
        if (LoadingScreenInstance)
        {
            GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(
                LoadingScreenInstance->TakeWidget(),
                100
            );
        }
    }
}

void UGameAssetManagerSubsystem::HideLoadingScreen()
{
    if (LoadingScreenInstance)
    {
        UGameInstance* GameInstance = GetGameInstance();
        if (GameInstance && GameInstance->GetGameViewportClient())
        {
            GameInstance->GetGameViewportClient()->RemoveViewportWidgetContent(
                LoadingScreenInstance->TakeWidget()
            );
        }

        LoadingScreenInstance->MarkAsGarbage();
        LoadingScreenInstance = nullptr;
    }
}

void UGameAssetManagerSubsystem::CleanupAfterLoad()
{
    HideLoadingScreen();

    CurrentLoadingPhase = ELoadingPhase::None;
    CurrentTeamToLoad.Empty();
    CurrentLevelToLoad.Reset();

    // 将所有待释放的句柄移入分帧释放队列
    PendingReleaseHandles.Append(MoveTemp(TeamAssetLoadHandles));
    TeamAssetLoadHandles.Empty();

    if (LevelLoadHandle.IsValid())
    {
        PendingReleaseHandles.Add(LevelLoadHandle);
        LevelLoadHandle.Reset();
    }

    // 启动分帧释放定时器（每 0.05s 释放一批，约每秒 20 批）
    if (!PendingReleaseHandles.IsEmpty())
    {
        UWorld* World = GetWorld();
        if (World && World->GetTimerManager().IsTimerActive(StaggeredReleaseTimerHandle))
        {
            World->GetTimerManager().ClearTimer(StaggeredReleaseTimerHandle);
        }
        if (World)
        {
            World->GetTimerManager().SetTimer(
                StaggeredReleaseTimerHandle,
                FTimerDelegate::CreateUObject(this, &UGameAssetManagerSubsystem::ReleaseHandlesStaggered),
                0.05f,   // 间隔 50ms
                true      // 循环
            );
        }
    }
    else
    {
        OnStaggeredReleaseComplete();
    }
}

void UGameAssetManagerSubsystem::ReleaseHandlesStaggered()
{
    int32 ReleasedThisFrame = 0;
    while (!PendingReleaseHandles.IsEmpty() && ReleasedThisFrame < HandlesToReleasePerFrame)
    {
        TSharedPtr<FStreamableHandle> Handle = PendingReleaseHandles.Pop();
        if (Handle.IsValid())
        {
            // 显式取消请求，释放引用计数
            Handle->CancelHandle();
        }
        ReleasedThisFrame++;
    }

    // 全部释放完毕
    if (PendingReleaseHandles.IsEmpty())
    {
        UWorld* World = GetWorld();
        if (World)
        {
            World->GetTimerManager().ClearTimer(StaggeredReleaseTimerHandle);
        }
        OnStaggeredReleaseComplete();
    }
}

void UGameAssetManagerSubsystem::OnStaggeredReleaseComplete()
{
    PendingReleaseHandles.Empty();
    UE_LOG(LogTemp, Log, TEXT("GameAssetManagerSubsystem: Staggered release complete. All handles freed."));
}
