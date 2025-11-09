// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Gameplay/TeamManager/TeamManagerSubsystem.h"
#include "Gameplay/CharacterManager/CharacterManagerSubsystem.h"
#include "Data/CharacterData.h"
#include "AbilitySystemComponent.h"

void UTeamManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (CharacterManagerClass)
    {
        CharacterManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
        if (!CharacterManager)
        {
            UE_LOG(LogTemp, Error, TEXT("TeamManagerSubsystem::Initialize - Failed to get CharacterManagerSubsystem."));
        }
        UE_LOG(LogTemp, Log, TEXT("TeamManagerSubsystem::Initialize - CharacterManagerSubsystem initialized."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TeamManagerSubsystem::Initialize - CharacterManagerClass is not set."));
	}
}

void UTeamManagerSubsystem::Deinitialize()
{
    CharacterManager = nullptr;

    Super::Deinitialize();
}

bool UTeamManagerSubsystem::SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex)
{
    if (!CharacterManager) return false;
    if (NewTeamCharacterTags.IsEmpty() || !NewTeamCharacterTags.IsValidIndex(NewActiveCharacterIndex)) return false;

    CurrentTeamCharacters = NewTeamCharacterTags;
    SetActiveCharacterIndex(NewActiveCharacterIndex);

    UE_LOG(LogTemp, Log, TEXT("Team set with %d members."), CurrentTeamCharacters.Num());
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

	// HandleAllCharactersDown(); 
}

bool UTeamManagerSubsystem::IsCharacterSwitchable(int32 Index) const
{

	if (!CurrentTeamCharacters.IsValidIndex(Index) || Index == ActiveCharacterIndex) return false;

    const FGameplayTag& CharacterTag = CurrentTeamCharacters[Index];
    if (!CharacterTag.IsValid()) return false;

    return true;
}