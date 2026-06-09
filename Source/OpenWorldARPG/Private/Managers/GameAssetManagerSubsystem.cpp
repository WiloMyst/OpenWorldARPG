// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Data/CharacterDataAsset.h"
#include "Data/CharacterInfoRow.h"
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
        float AssetProgressSum = 0.0f;
        if (!TeamAssetLoadHandles.IsEmpty())
        {
            for (const auto& Handle : TeamAssetLoadHandles)
            {
                if (Handle.IsValid())
                {
                    AssetProgressSum += Handle->GetProgress();
                }
            }
            float AssetPhaseProgress = AssetProgressSum / TeamAssetLoadHandles.Num();
            Progress = LevelLoadWeight + (AssetPhaseProgress * TeamAssetLoadWeight);
        }
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

    TArray<FSoftObjectPath> AssetPathsToLoad;
    UDataTable* LoadedTable = GetCharacterInfoTable();
    UCharacterManagerSubsystem* CharManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    if (LoadedTable && CharManager)
    {
        for (const FGameplayTag& CharTag : CurrentTeamToLoad)
        {
            FName RowName = CharManager->GetRowNameByTag(CharTag);
            if (RowName == NAME_None) continue;

            FCharacterInfoRow* Row = LoadedTable->FindRow<FCharacterInfoRow>(RowName, TEXT(""));
            if (Row && Row->CharacterDataAsset)
            {
                UCharacterDataAsset* DataAsset = Row->CharacterDataAsset;
                if (DataAsset)
                {
                    AssetPathsToLoad.Add(DataAsset->HeadIcon.ToSoftObjectPath());
                    AssetPathsToLoad.Add(DataAsset->SplashArt.ToSoftObjectPath());
                    AssetPathsToLoad.Add(DataAsset->CharacterMesh.ToSoftObjectPath());

                    AssetPathsToLoad.Add(DataAsset->NormalAttack.Icon.ToSoftObjectPath());
                    for (TSoftObjectPtr<UAnimMontage> Montage : DataAsset->NormalAttack.Montages)
                    {
                        AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                    }
                    AssetPathsToLoad.Add(DataAsset->HeavyAttack.Icon.ToSoftObjectPath());
                    for (TSoftObjectPtr<UAnimMontage> Montage : DataAsset->HeavyAttack.Montages)
                    {
                        AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                    }
                    AssetPathsToLoad.Add(DataAsset->PlungeAttack.Icon.ToSoftObjectPath());
                    for (TSoftObjectPtr<UAnimMontage> Montage : DataAsset->PlungeAttack.Montages)
                    {
                        AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                    }
                    AssetPathsToLoad.Add(DataAsset->SkillAttack.Icon.ToSoftObjectPath());
                    for (TSoftObjectPtr<UAnimMontage> Montage : DataAsset->SkillAttack.Montages)
                    {
                        AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                    }
                    AssetPathsToLoad.Add(DataAsset->UltimateAttack.Icon.ToSoftObjectPath());
                    for (TSoftObjectPtr<UAnimMontage> Montage : DataAsset->UltimateAttack.Montages)
                    {
                        AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                    }
                    for (const FTalentConfig& Talent : DataAsset->PassiveTalents)
                    {
                        AssetPathsToLoad.Add(Talent.Icon.ToSoftObjectPath());
                        for (TSoftObjectPtr<UAnimMontage> Montage : Talent.Montages)
                        {
                            AssetPathsToLoad.Add(Montage.ToSoftObjectPath());
                        }
                    }
                }
            }
        }
    }

    TeamAssetLoadNum = AssetPathsToLoad.Num();
    if (TeamAssetLoadNum == 0)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

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
