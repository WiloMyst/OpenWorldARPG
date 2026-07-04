// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/GameplayGameModeBase.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Characters/PlayerCharacter/Data/CharacterVisualDataAsset.h"
#include "Systems/CombatSystem/Data/CharacterCombatDataAsset.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
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
}

void AGameplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!HasAuthority()) return;

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

    UCharacterManagerSubsystem* CharManager = nullptr;
    UTeamManagerSubsystem* TeamManager = nullptr;

    // TODO: [Network Architecture] 联机模式下应通过 UniqueNetId 向 ServerDataManager 请求队伍数据
    if (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer)
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            CharManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
            TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>();
        }
    }

    if (!CharManager || !TeamManager) return;

    const TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();

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

        const FCharacterSaveData* SaveDataPtr = CharManager->GetCharacterSaveData(CharacterTag);
        if (!SaveDataPtr) continue;

        FCharacterRegistryRow RegistryRow;
        if (!CharManager->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow)) continue;

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

    int32 ActiveIndex = TeamManager->GetActiveCharacterIndex();
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

    UCharacterManagerSubsystem* CharManager = nullptr;

    // TODO: [Network Architecture] 联机模式下应通过 UniqueNetId 向 ServerDataManager 请求角色数据
    if (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer)
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            CharManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
        }
    }

    if (!CharManager) return;

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

        const FCharacterSaveData* SaveDataPtr = CharManager->GetCharacterSaveData(CharacterTag);
        if (!SaveDataPtr) continue;

        FCharacterRegistryRow RegistryRow;
        if (!CharManager->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow)) continue;

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
