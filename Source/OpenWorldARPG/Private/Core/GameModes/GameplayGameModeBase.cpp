// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/GameplayGameModeBase.h"
#include "Core/GameStates/OpenWorldARPGGameStateBase.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Characters/PlayerCharacter/Data/CharacterVisualDataAsset.h"
#include "Systems/CombatSystem/Data/CharacterCombatDataAsset.h"
#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Systems/CharacterManager/ServerPlayerDataManager.h"
#include "Systems/TeamManager/TeamManagerSubsystem.h"
#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Systems/GameFlowManager/GameFlowSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

AGameplayGameModeBase::AGameplayGameModeBase()
{
    DefaultPlayerStartTag = FName("PlayerStart");
    AssetCleanupDelay = 3.0f;

    GameStateClass = AOpenWorldARPGGameStateBase::StaticClass();
}

void AGameplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!HasAuthority()) return;

    // 服务器侧：初始化该玩家的存档数据（从模拟数据库/真实后端加载）
    if (AOpenWorldARPGGameStateBase* GS = GetGameState<AOpenWorldARPGGameStateBase>())
    {
        if (UServerPlayerDataManager* PlayerDataManager = GS->GetServerPlayerDataManager())
        {
            PlayerDataManager->OnPlayerConnected(NewPlayer);
        }
    }

    GeneratePlayerCharacters(NewPlayer);

    if (UGameFlowSubsystem* FlowManager = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>())
    {
        FlowManager->NotifyNewLevelReady();
    }

    if (!CleanupTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CleanupTimerHandle, this, &AGameplayGameModeBase::CleanupAfterLoad, AssetCleanupDelay, false);
    }
}

void AGameplayGameModeBase::GeneratePlayerCharacters(APlayerController* PlayerController)
{
    AGameplayPlayerState* PlayerState = PlayerController->GetPlayerState<AGameplayPlayerState>();
    if (!PlayerState) return;

    // 服务器侧：从 UServerPlayerDataManager 读取该玩家的存档（按 UniqueNetId 索引）
    UServerPlayerDataManager* PlayerDataManager = nullptr;
    if (AOpenWorldARPGGameStateBase* GS = GetGameState<AOpenWorldARPGGameStateBase>())
    {
        PlayerDataManager = GS->GetServerPlayerDataManager();
    }
    if (!PlayerDataManager) return;

    // 全局注册表查询：从 UCharacterRegistrySubsystem 读取（所有玩家共用）
    UCharacterRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UCharacterRegistrySubsystem>();

    // 队伍 Tag 列表：服务器侧通过 PlayerState 或数据管理器获取。
    // 单机/ListenServer 下仍从本地 UTeamManagerSubsystem 读取（主机既是服务器也是客户端）。
    TArray<FGameplayTag> TeamTags;
    if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
    {
        if (UTeamManagerSubsystem* TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
        {
            TeamTags = TeamManager->GetCurrentTeamCharacterTags();
        }
    }
    // [DB-INTEGRATION] Dedicated Server 下 TeamTags 应从该玩家的存档中读取，
    // 而非本地 UTeamManagerSubsystem（DS 上无 LocalPlayer）。当前模拟阶段复用主机本地配置。
    if (TeamTags.IsEmpty()) return;

    AActor* StartSpot = FindPlayerStart(PlayerController, DefaultPlayerStartTag.ToString());
    FTransform SpawnTransform = StartSpot ? StartSpot->GetActorTransform() : FTransform::Identity;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = PlayerController;

    TArray<APlayerCharacter*> TeamActors;
    TeamActors.SetNum(TeamTags.Num());

    for (int32 TeamIndex = 0; TeamIndex < TeamTags.Num(); ++TeamIndex)
    {
        const FGameplayTag& CharacterTag = TeamTags[TeamIndex];

        // 玩家队伍存档：从服务器侧数据管理器读取（单玩家，按角色 Tag）
        const FCharacterSaveData* SaveDataPtr = PlayerDataManager->GetCharacterSaveData(PlayerController, CharacterTag);
        if (!SaveDataPtr) continue;

        // 全局注册表行：从 UCharacterRegistrySubsystem 读取
        FCharacterRegistryRow RegistryRow;
        if (!Registry || !Registry->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow)) continue;

        UCharacterVisualDataAsset* VisualData = RegistryRow.VisualData.LoadSynchronous();
        UCharacterCombatDataAsset* CombatData = RegistryRow.CombatData.LoadSynchronous();
        if (!VisualData || !CombatData) continue;

        UClass* ClassToSpawn = VisualData->CharacterBlueprint.IsValid()
            ? VisualData->CharacterBlueprint.Get()
            : VisualData->CharacterBlueprint.LoadSynchronous();
        if (!ClassToSpawn) continue;

        APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(ClassToSpawn, SpawnTransform, SpawnParams);
        if (SpawnedChar)
        {
            SpawnedChar->InitializeCharacter(*SaveDataPtr, VisualData, CombatData, RegistryRow);
            TeamActors[TeamIndex] = SpawnedChar;
            SpawnedChar->SetStandbyMode(true);
        }
    }

    PlayerState->SetTeamCharacterActors(TeamActors);

    // 激活角色索引：服务器侧从数据管理器或 PlayerState 读取
    int32 ActiveIndex = 0;
    if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
    {
        if (UTeamManagerSubsystem* TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
        {
            ActiveIndex = TeamManager->GetActiveCharacterIndex();
        }
    }
    if (TeamActors.IsValidIndex(ActiveIndex) && TeamActors[ActiveIndex])
    {
        TeamActors[ActiveIndex]->SetStandbyMode(false);
        PlayerController->Possess(TeamActors[ActiveIndex]);
        PlayerState->SetActiveCharacterIndex(ActiveIndex);
    }
}

void AGameplayGameModeBase::CleanupAfterLoad()
{
    if (UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>())
    {
        AssetManager->CleanupAfterLoad();
    }
}

void AGameplayGameModeBase::ApplyPlayerTeamChanges(AGameplayPlayerController* PlayerController, const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex)
{
    if (!HasAuthority() || !PlayerController) return;

    // 服务器侧：从 UServerPlayerDataManager 读取该玩家存档
    UServerPlayerDataManager* PlayerDataManager = nullptr;
    if (AOpenWorldARPGGameStateBase* GS = GetGameState<AOpenWorldARPGGameStateBase>())
    {
        PlayerDataManager = GS->GetServerPlayerDataManager();
    }
    if (!PlayerDataManager) return;

    // 全局注册表查询
    UCharacterRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UCharacterRegistrySubsystem>();

    AGameplayPlayerState* PlayerState = PlayerController->GetPlayerState<AGameplayPlayerState>();
    if (!PlayerState) return;

    // 记录旧角色位置
    FTransform SpawnTransform = FTransform::Identity;
    if (APlayerCharacter* OldActiveChar = Cast<APlayerCharacter>(PlayerController->GetPawn()))
    {
        SpawnTransform = OldActiveChar->GetActorTransform();
    }
    else if (AActor* FallbackStart = FindPlayerStart(PlayerController, DefaultPlayerStartTag.ToString()))
    {
        SpawnTransform = FallbackStart->GetActorTransform();
    }

    // 销毁旧队伍
    TArray<APlayerCharacter*> OldTeam;
    PlayerState->GetAllTeamCharacters(OldTeam);
    for (APlayerCharacter* OldChar : OldTeam)
    {
        if (IsValid(OldChar))
        {
            if (PlayerController->GetPawn() == OldChar)
            {
                PlayerController->UnPossess();
            }
            OldChar->Destroy();
        }
    }

    // 生成新队伍
    TArray<APlayerCharacter*> NewTeamActors;
    NewTeamActors.SetNum(NewTeamTags.Num());

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = PlayerController;

    for (int32 TeamIndex = 0; TeamIndex < NewTeamTags.Num(); ++TeamIndex)
    {
        const FGameplayTag& CharacterTag = NewTeamTags[TeamIndex];
        if (!CharacterTag.IsValid()) continue;

        // 玩家队伍存档：从服务器侧数据管理器读取（单玩家，按角色 Tag）
        const FCharacterSaveData* SaveDataPtr = PlayerDataManager->GetCharacterSaveData(PlayerController, CharacterTag);
        if (!SaveDataPtr) continue;

        // 全局注册表行：从 UCharacterRegistrySubsystem 读取
        FCharacterRegistryRow RegistryRow;
        if (!Registry || !Registry->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow)) continue;

        UCharacterVisualDataAsset* VisualData = RegistryRow.VisualData.LoadSynchronous();
        UCharacterCombatDataAsset* CombatData = RegistryRow.CombatData.LoadSynchronous();
        if (!VisualData || !CombatData) continue;

        UClass* ClassToSpawn = VisualData->CharacterBlueprint.IsValid()
            ? VisualData->CharacterBlueprint.Get()
            : VisualData->CharacterBlueprint.LoadSynchronous();
        if (!ClassToSpawn) continue;

        APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(ClassToSpawn, SpawnTransform, SpawnParams);
        if (!SpawnedChar) continue;

        SpawnedChar->InitializeCharacter(*SaveDataPtr, VisualData, CombatData, RegistryRow);
        SpawnedChar->SetStandbyMode(TeamIndex != ActiveIndex);
        NewTeamActors[TeamIndex] = SpawnedChar;
    }

    PlayerState->SetTeamCharacterActors(NewTeamActors);

    if (NewTeamActors.IsValidIndex(ActiveIndex) && NewTeamActors[ActiveIndex])
    {
        NewTeamActors[ActiveIndex]->SetStandbyMode(false);
        PlayerController->Possess(NewTeamActors[ActiveIndex]);
        PlayerState->SetActiveCharacterIndex(ActiveIndex);
    }
}
