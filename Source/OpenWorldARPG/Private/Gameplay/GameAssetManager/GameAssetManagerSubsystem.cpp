// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/GameAssetManager/GameAssetManagerSubsystem.h"
#include "Data/CharacterDataAsset.h"
#include "Data/CharacterInfoRow.h"
#include "UI/LoadingScreenWidget.h"
#include "Utils/GASBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"

void UGameAssetManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UAssetManager& AssetManager = UAssetManager::Get();
    StreamableManager = &AssetManager.GetStreamableManager();

}

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
            // 计算资产加载阶段的自身进度 (0-1)
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

    UE_LOG(LogTemp, Log, TEXT("Total Loading Progress: %f"), Progress);
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

    // 1. 设置状态
    CurrentLevelToLoad = LevelToLoad;
    TargetLevelName = FName(*FPackageName::GetShortName(LevelToLoad.GetLongPackageName()));

    // 2. 显示加载界面
    ShowLoadingScreen();

    // 3. 开始异步加载关卡
    StartLevelLoading();
}

// ####################################################################
// #                          阶段一：关卡加载                           #
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

// ####################################################################
// #                        阶段二：队伍资产加载                         #
// ####################################################################

void UGameAssetManagerSubsystem::StartTeamAssetLoading(const TArray<FGameplayTag>& TeamCharacterTags)
{
    CurrentLoadingPhase = ELoadingPhase::LoadingTeamAssets;
    TeamAssetLoadHandles.Empty();
    CompletedAssetLoads = 0;
	FailedAssetLoads = 0;

    CurrentTeamToLoad = TeamCharacterTags;

    // 收集所有需要加载的资产路径
    TArray<FSoftObjectPath> AssetPathsToLoad;
    if (CharacterInfoTable.IsValid())
    {
        UDataTable* LoadedTable = CharacterInfoTable.LoadSynchronous();
        if (LoadedTable)
        {
            for (const FGameplayTag& CharTag : CurrentTeamToLoad)
            {
                FName RowName = FName(UGASBlueprintLibrary::GetLastPartOfGameplayTag(CharTag));
                FCharacterInfoRow* Row = LoadedTable->FindRow<FCharacterInfoRow>(RowName, TEXT(""));
                if (Row && Row->CharacterDataAsset)
                {
                    // 加载DataAsset内部引用的所有重资产SoftObjectPtr
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
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("DataAsset is null for Tag: %s."), *CharTag.ToString());
					}
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Could not find row or valid DataAsset for Tag: %s."), *CharTag.ToString());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to load CharacterInfoTable."));
		}
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CharacterInfoTable is not valid! Cannot load team assets."));
	}

    TeamAssetLoadNum = AssetPathsToLoad.Num();
    if (TeamAssetLoadNum == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No team assets to load. Skipping to next phase."));
        OnAllTeamAssetsLoaded();
        return;
    }
    
    // 为每个资产单独发起请求，以便跟踪进度
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
    UE_LOG(LogTemp, Log, TEXT("No.%d team asset async loaded successfully!"), CompletedAssetLoads);
    if (CompletedAssetLoads + FailedAssetLoads >= TeamAssetLoadNum)
    {
        OnAllTeamAssetsLoaded();
    }
}

void UGameAssetManagerSubsystem::OnAllTeamAssetsLoaded()
{
    TeamAssetLoadNum = 0;
    TeamAssetLoadHandles.Empty();
    CurrentLoadingPhase = ELoadingPhase::Complete;
    UE_LOG(LogTemp, Log, TEXT("Phase 2 Complete: All team assets loaded."));

    UGameplayStatics::OpenLevel(GetWorld(), TargetLevelName);

    // 广播总完成事件
    //OnLoadComplete.Broadcast();
}

// ####################################################################
// #                          UI 和清理辅助函数                          #
// ####################################################################

void UGameAssetManagerSubsystem::ShowLoadingScreen()
{
    // 服务器不需要创建UI
    if (GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (LoadingScreenWidgetClass && !LoadingScreenInstance)
    {
        LoadingScreenInstance = CreateWidget<ULoadingScreenWidget>(GetGameInstance(), LoadingScreenWidgetClass);
        if (LoadingScreenInstance)
        {
            // 使用GameInstance的Viewport Client来添加UI
            GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(
                LoadingScreenInstance->TakeWidget(),
                100 // Z-Order
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

        LoadingScreenInstance->MarkAsGarbage(); // 标记为待回收
        LoadingScreenInstance = nullptr;
    }
}

void UGameAssetManagerSubsystem::CleanupAfterLoad()
{
    HideLoadingScreen();

    // 重置所有状态变量
    CurrentLoadingPhase = ELoadingPhase::None;
    CurrentTeamToLoad.Empty();
    CurrentLevelToLoad.Reset();

    // 清理所有加载句柄
    TeamAssetLoadHandles.Empty();
    if (LevelLoadHandle.IsValid())
    {
        LevelLoadHandle.Reset();
    }

    UE_LOG(LogTemp, Log, TEXT("GameAssetManagerSubsystem has been cleaned up after level load."));
}

