// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Managers/TeamManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Data/StartingRosterConfig.h"
#include "Engine/GameInstance.h"

void UTeamManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CharacterManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    if (!CharacterManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TeamManagerSubsystem::Initialize - Failed to get CharacterManagerSubsystem."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("TeamManagerSubsystem::Initialize - CharacterManagerSubsystem linked."));
    }
}

void UTeamManagerSubsystem::Deinitialize()
{
    CharacterManager = nullptr;
    CurrentTeamCharacters.Empty();

    Super::Deinitialize();
}

void UTeamManagerSubsystem::InitializeFromDataObject(UObject* InDataObject)
{
    UStartingRosterConfig* ConfigData = Cast<UStartingRosterConfig>(InDataObject);

    if (!ConfigData)
    {
        UE_LOG(LogTemp, Warning, TEXT("TeamManager InitializeFromDataObject: Cast Failed!"));
        return;
    }

    SetCurrentTeam(ConfigData->InitialTeamTags, ConfigData->InitialActiveCharacterIndex);
}

void UTeamManagerSubsystem::SwitchToCharacterByIndex(int32 TeamIndex)
{
    if (IsCharacterSwitchable(TeamIndex))
    {
        OnRequestCharacterSwitch.Broadcast(TeamIndex);

        UE_LOG(LogTemp, Log, TEXT("TeamManager: 请求切换到索引 %d 的角色"), TeamIndex);
    }
}

bool UTeamManagerSubsystem::SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex)
{
    if (!CharacterManager) return false;

    int32 SafeIndex = NewTeamCharacterTags.IsValidIndex(NewActiveCharacterIndex) ? NewActiveCharacterIndex : 0;

    if (NewTeamCharacterTags.IsEmpty()) return false;

    CurrentTeamCharacters = NewTeamCharacterTags;
    SetActiveCharacterIndex(SafeIndex);

    UE_LOG(LogTemp, Log, TEXT("Team set with %d members. Active Index: %d"), CurrentTeamCharacters.Num(), SafeIndex);
    OnTeamMembersChanged.Broadcast();
    return true;
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
    if (!CharacterTag.IsValid()) return false;

    return true;
}