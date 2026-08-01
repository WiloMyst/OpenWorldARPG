// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Characters/PlayerCharacter/Data/CharacterVisualDataAsset.h"
#include "Systems/CombatSystem/Data/CharacterCombatDataAsset.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Characters/PlayerCharacter/Data/CharacterGeneralDataAsset.h"
#include "Core/Data/UIDataAsset.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemGlobals.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"

void UGameAssetManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UAbilitySystemGlobals::Get().InitGlobalData();

    UAssetManager& AssetManager = UAssetManager::Get();
    StreamableManager = &AssetManager.GetStreamableManager();
}

// --- 资产缓存访问 ---

UDataTable* UGameAssetManagerSubsystem::GetCharacterInfoTable()
{
    if (!CachedCharacterInfoTable)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        CachedCharacterInfoTable = Settings.CharacterInfoTable.LoadSynchronous();
        if (!CachedCharacterInfoTable)
        {
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: CharacterInfoTable 未配置或加载失败！"));
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
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: ItemDatabaseTable 未配置或加载失败！"));
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
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: InventoryCategoryTabDataTable 未配置或加载失败！"));
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
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: PlayerCharacterGeneralAbilityDataAsset 未配置或加载失败！"));
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
            UE_LOG(LogTemp, Error, TEXT("GameAssetManager: UIMapDataAsset 未配置或加载失败！"));
        }
    }
    return CachedUIMapDataAsset;
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
        float AssetPhaseProgress = 0.0f;

        // 阶段 2a：数据资产加载进度
        if (DataAssetLoadHandle.IsValid())
        {
            AssetPhaseProgress += DataAssetLoadHandle->GetProgress() * DataAssetPhaseWeight;
        }
        else
        {
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
        Progress = 1.0f;
        break;
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

    StartLevelLoading();
}

// --- 阶段一：关卡加载 ---

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
        OnLevelLoadCompleted();
    }
}

void UGameAssetManagerSubsystem::OnLevelLoadCompleted()
{
    OnLevelAsyncLoaded.Broadcast();
}

// --- 阶段二：队伍资产加载 ---

void UGameAssetManagerSubsystem::StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags)
{
    CurrentLoadingPhase = ELoadingPhase::LoadingTeamAssets;
    TeamAssetLoadHandles.Empty();
    CompletedAssetLoads = 0;
    FailedAssetLoads = 0;
    CurrentTeamToLoad = TeamCharacterTags;

    // 两阶段加载：先异步加载数据资产本身，再加载其内部软引用资源
    StartDataAssetLoading();
}

void UGameAssetManagerSubsystem::StartDataAssetLoading()
{
    TArray<FSoftObjectPath> DataAssetPaths;

    UDataTable* LoadedTable = GetCharacterInfoTable();
    if (!LoadedTable)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

    UCharacterRegistrySubsystem* Registry = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        Registry = GI->GetSubsystem<UCharacterRegistrySubsystem>();
    }

    if (!Registry)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

    for (const FGameplayTag& CharTag : CurrentTeamToLoad)
    {
        FName RowName = Registry->GetRowNameByTag(CharTag);
        if (RowName == NAME_None) continue;

        const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
        if (Row)
        {
            DataAssetPaths.Add(Row->VisualData.ToSoftObjectPath());
            DataAssetPaths.Add(Row->CombatData.ToSoftObjectPath());
        }
    }

    if (DataAssetPaths.Num() == 0)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

    DataAssetLoadHandle = StreamableManager->RequestAsyncLoad(
        DataAssetPaths,
        FStreamableDelegate::CreateUObject(this, &UGameAssetManagerSubsystem::OnDataAssetsLoaded)
    );

    if (!DataAssetLoadHandle.IsValid())
    {
        OnAllTeamAssetsLoaded();
    }
}

void UGameAssetManagerSubsystem::OnDataAssetsLoaded()
{
    // 2a 句柄随 2b 一起分帧释放
    if (DataAssetLoadHandle.IsValid())
    {
        TeamAssetLoadHandles.Add(DataAssetLoadHandle);
        DataAssetLoadHandle.Reset();
    }

    StartInnerAssetLoading();
}

void UGameAssetManagerSubsystem::StartInnerAssetLoading()
{
    TArray<FSoftObjectPath> AssetPathsToLoad;

    UDataTable* LoadedTable = GetCharacterInfoTable();
    if (!LoadedTable)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

    UCharacterRegistrySubsystem* Registry = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        Registry = GI->GetSubsystem<UCharacterRegistrySubsystem>();
    }

    if (!Registry)
    {
        OnAllTeamAssetsLoaded();
        return;
    }

    for (const FGameplayTag& CharTag : CurrentTeamToLoad)
    {
        FName RowName = Registry->GetRowNameByTag(CharTag);
        if (RowName == NAME_None) continue;

        const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
        if (Row)
        {
            // UI 资源
            AssetPathsToLoad.Add(Row->HeadIcon.ToSoftObjectPath());
            AssetPathsToLoad.Add(Row->SplashArt.ToSoftObjectPath());

            // 外观资产
            if (UCharacterVisualDataAsset* VisualData = Row->VisualData.Get())
            {
                AssetPathsToLoad.Add(VisualData->CharacterMesh.ToSoftObjectPath());
                AssetPathsToLoad.Add(VisualData->AnimationBlueprint.ToSoftObjectPath());
                AssetPathsToLoad.Add(VisualData->UpperBodyLayers.ToSoftObjectPath());
                AssetPathsToLoad.Add(VisualData->WeaponBlueprint.ToSoftObjectPath());
                AssetPathsToLoad.Add(VisualData->ClimbUpMontage.ToSoftObjectPath());
                AssetPathsToLoad.Add(VisualData->GrappleMontage.ToSoftObjectPath());
            }

            // 战斗资产
            if (UCharacterCombatDataAsset* CombatData = Row->CombatData.Get())
            {
                for (const auto& TalentPair : CombatData->CharacterTalents)
                {
                    const FTalentConfig& Talent = TalentPair.Value;
                    AssetPathsToLoad.Add(Talent.Icon.ToSoftObjectPath());

                    for (const auto& NodePair : Talent.ComboGraph)
                    {
                        AssetPathsToLoad.Add(NodePair.Value.Montage.ToSoftObjectPath());
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
    // 句柄保持持有引用，直到 CleanupAfterLoad 分帧释放
    TeamAssetLoadNum = 0;
    CurrentLoadingPhase = ELoadingPhase::Complete;

    OnLoadComplete.Broadcast();
}

// --- 清理 ---

void UGameAssetManagerSubsystem::CleanupAfterLoad()
{
    CurrentLoadingPhase = ELoadingPhase::None;
    CurrentTeamToLoad.Empty();
    CurrentLevelToLoad.Reset();

    PendingReleaseHandles.Append(MoveTemp(TeamAssetLoadHandles));
    TeamAssetLoadHandles.Empty();

    if (LevelLoadHandle.IsValid())
    {
        PendingReleaseHandles.Add(LevelLoadHandle);
        LevelLoadHandle.Reset();
    }

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
                0.05f,
                true
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
            Handle->CancelHandle();
        }
        ReleasedThisFrame++;
    }

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
}
