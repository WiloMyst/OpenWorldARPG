// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/TeamManager/TeamManagerSubsystem.h"
#include "Gameplay/CharacterManager/CharacterManagerSubsystem.h"
#include "Data/CharacterData.h"
#include "AbilitySystemComponent.h"

void UTeamManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (CharacterManagerClass)
    {
        const TArray<UCharacterManagerSubsystem*> Subsystems = GetWorld()->GetSubsystemArray<UCharacterManagerSubsystem>();
        for (UCharacterManagerSubsystem* Subsystem : Subsystems)
        {
            if (Subsystem && Subsystem->GetClass() == CharacterManagerClass)
            {
                CharacterManager = Subsystem;
                UE_LOG(LogTemp, Log, TEXT("CharacterManager from TeamManagerSubsystem set up!"));
                break;
            }
        }
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

    ACharacterData* CharacterData = CharacterManager->GetOwnedCharacterDataByTag(CharacterTag);
    if (!CharacterData) return false;

    UAbilitySystemComponent* ASC = CharacterData->GetAbilitySystemComponent();
    if (!ASC) return false;

    // 检查是否拥有不可以进行角色切换的状态标签
    FGameplayTagContainer UnswitchableStates;
    UnswitchableStates.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Dead")));
    UnswitchableStates.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Unswitchable")));

    return !ASC->HasAnyMatchingGameplayTags(UnswitchableStates);
}