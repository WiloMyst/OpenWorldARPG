// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/TeamManager/TeamManagerSubsystem.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Core/Data/InitialArchiveData.h"
#include "Engine/LocalPlayer.h"

void UTeamManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CharacterManager = GetLocalPlayer()->GetSubsystem<UCharacterManagerSubsystem>();
    if (!CharacterManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TeamManagerSubsystem: Failed to get CharacterManagerSubsystem."));
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = GetLocalPlayer()->GetPlayerController(World))
        {
            if (AGameplayPlayerState* PlayerState = PC->GetPlayerState<AGameplayPlayerState>())
            {
                BindToPlayerStateEvents(PlayerState);
            }
        }
    }
}

void UTeamManagerSubsystem::Deinitialize()
{
    if (BoundPlayerState.IsValid())
    {
        BoundPlayerState->OnActiveCharacterIndexChanged.RemoveDynamic(this, &UTeamManagerSubsystem::HandleActiveCharacterIndexChanged);
        BoundPlayerState->OnTeamCharacterActorsChanged.RemoveDynamic(this, &UTeamManagerSubsystem::HandleTeamCharacterActorsChanged);
        BoundPlayerState = nullptr;
    }

    CharacterManager = nullptr;
    CurrentTeamCharacters.Empty();

    Super::Deinitialize();
}

void UTeamManagerSubsystem::InitializeFromDataObject(UObject* InDataObject)
{
    UInitialArchiveData* ConfigData = Cast<UInitialArchiveData>(InDataObject);
    if (!ConfigData) return;

    SetCurrentTeam(ConfigData->InitialTeamTags, ConfigData->InitialActiveCharacterIndex);
}

void UTeamManagerSubsystem::InitializeFromServerData(const TArray<FGameplayTag>& TeamTags, int32 InActiveCharacterIndex)
{
    SetCurrentTeam(TeamTags, InActiveCharacterIndex);
}

bool UTeamManagerSubsystem::SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex)
{
    if (!CharacterManager || NewTeamCharacterTags.IsEmpty()) return false;

    int32 SafeIndex = NewTeamCharacterTags.IsValidIndex(NewActiveCharacterIndex) ? NewActiveCharacterIndex : 0;

    CurrentTeamCharacters = NewTeamCharacterTags;
    SetActiveCharacterIndex(SafeIndex);

    OnTeamListUpdatedDelegate.Broadcast();
    return true;
}

void UTeamManagerSubsystem::SwitchToCharacterByIndex(int32 TeamIndex)
{
    if (!IsCharacterSwitchable(TeamIndex)) return;

    if (UWorld* World = GetWorld())
    {
        if (AGameplayPlayerController* PC = Cast<AGameplayPlayerController>(GetLocalPlayer()->GetPlayerController(World)))
        {
            PC->Server_SwitchCharacter(TeamIndex);
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("TeamManager: SwitchToCharacterByIndex - 无法获取本地 PlayerController！"));
}

void UTeamManagerSubsystem::SwitchToCharacterByTag(const FGameplayTag& CharacterTag)
{
    int32 FoundIndex = CurrentTeamCharacters.Find(CharacterTag);
    if (FoundIndex != INDEX_NONE)
    {
        SwitchToCharacterByIndex(FoundIndex);
    }
}

void UTeamManagerSubsystem::CycleToNextCharacter()
{
    if (CurrentTeamCharacters.Num() <= 1) return;

    for (int32 i = 1; i < CurrentTeamCharacters.Num(); ++i)
    {
        const int32 NextIndex = (ActiveCharacterIndex + i) % CurrentTeamCharacters.Num();
        if (IsCharacterSwitchable(NextIndex))
        {
            SwitchToCharacterByIndex(NextIndex);
            return;
        }
    }
}

bool UTeamManagerSubsystem::IsCharacterSwitchable(int32 Index) const
{
    if (!CurrentTeamCharacters.IsValidIndex(Index) || Index == ActiveCharacterIndex) return false;

    const FGameplayTag& CharacterTag = CurrentTeamCharacters[Index];
    return CharacterTag.IsValid();
}

void UTeamManagerSubsystem::OnRep_ActiveCharacterIndexFromServer(int32 NewActiveIndex)
{
    int32 OldIndex = ActiveCharacterIndex;
    ActiveCharacterIndex = NewActiveIndex;

    FGameplayTag OldTag = CurrentTeamCharacters.IsValidIndex(OldIndex) ? CurrentTeamCharacters[OldIndex] : FGameplayTag::EmptyTag;
    FGameplayTag NewTag = CurrentTeamCharacters.IsValidIndex(NewActiveIndex) ? CurrentTeamCharacters[NewActiveIndex] : FGameplayTag::EmptyTag;

    OnActiveCharacterChanged.Broadcast(OldTag, NewTag);
}

void UTeamManagerSubsystem::OnRep_TeamCharacterActorsFromServer(const TArray<APlayerCharacter*>& NewTeamActors)
{
    TArray<FGameplayTag> UpdatedTags;
    UpdatedTags.Reserve(NewTeamActors.Num());

    for (APlayerCharacter* Character : NewTeamActors)
    {
        if (IsValid(Character))
        {
            UpdatedTags.Add(Character->GetCharacterTag());
        }
    }

    if (UpdatedTags != CurrentTeamCharacters)
    {
        CurrentTeamCharacters = MoveTemp(UpdatedTags);
        OnTeamListUpdatedDelegate.Broadcast();
    }
}

void UTeamManagerSubsystem::BindToPlayerStateEvents(AGameplayPlayerState* PlayerState)
{
    if (!IsValid(PlayerState)) return;

    PlayerState->OnActiveCharacterIndexChanged.AddDynamic(
        this, &UTeamManagerSubsystem::HandleActiveCharacterIndexChanged);

    PlayerState->OnTeamCharacterActorsChanged.AddDynamic(
        this, &UTeamManagerSubsystem::HandleTeamCharacterActorsChanged);

    BoundPlayerState = PlayerState;
}

void UTeamManagerSubsystem::HandleActiveCharacterIndexChanged(int32 OldIndex, int32 NewIndex)
{
    OnRep_ActiveCharacterIndexFromServer(NewIndex);
}

void UTeamManagerSubsystem::HandleTeamCharacterActorsChanged()
{
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = GetLocalPlayer()->GetPlayerController(World))
        {
            if (AGameplayPlayerState* PlayerState = PC->GetPlayerState<AGameplayPlayerState>())
            {
                OnRep_TeamCharacterActorsFromServer(PlayerState->GetTeamCharacterActors());
            }
        }
    }
}
