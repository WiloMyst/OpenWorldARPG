// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Characters/PlayerCharacter.h"
#include "Net/UnrealNetwork.h"

AGameplayPlayerState::AGameplayPlayerState()
{
}

void AGameplayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AGameplayPlayerState, TeamCharacterActors, COND_OwnerOnly);
	DOREPLIFETIME(AGameplayPlayerState, ActiveCharacterIndex);
}

APlayerCharacter* AGameplayPlayerState::GetTeamCharacterByIndex(int32 Index) const
{
	if (TeamCharacterActors.IsValidIndex(Index))
	{
		APlayerCharacter* Character = TeamCharacterActors[Index];
		return IsValid(Character) ? Character : nullptr;
	}
	return nullptr;
}

APlayerCharacter* AGameplayPlayerState::GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const
{
	for (APlayerCharacter* Character : TeamCharacterActors)
	{
		if (IsValid(Character) && Character->GetCharacterTag() == CharacterTag)
		{
			return Character;
		}
	}
	return nullptr;
}

void AGameplayPlayerState::GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const
{
	OutCharacters.Empty();
	OutCharacters.Reserve(TeamCharacterActors.Num());
	for (APlayerCharacter* Character : TeamCharacterActors)
	{
		if (IsValid(Character))
		{
			OutCharacters.Add(Character);
		}
	}
}

void AGameplayPlayerState::SetTeamCharacterActors(const TArray<APlayerCharacter*>& InActors)
{
	if (HasAuthority())
	{
		TeamCharacterActors = InActors;
	}
}

void AGameplayPlayerState::AddTeamCharacter(APlayerCharacter* InCharacter)
{
	if (HasAuthority() && IsValid(InCharacter))
	{
		TeamCharacterActors.Add(InCharacter);
	}
}

void AGameplayPlayerState::SetActiveCharacterIndex(int32 NewIndex)
{
	if (HasAuthority())
	{
		int32 OldIndex = ActiveCharacterIndex;
		ActiveCharacterIndex = NewIndex;

		if (OldIndex != NewIndex)
		{
			OnActiveCharacterIndexChanged.Broadcast(OldIndex, NewIndex);
		}
	}
}

void AGameplayPlayerState::OnRep_ActiveCharacterIndex(int32 OldIndex)
{
	OnActiveCharacterIndexChanged.Broadcast(OldIndex, ActiveCharacterIndex);
}

void AGameplayPlayerState::OnRep_TeamCharacterActors()
{
	OnTeamCharacterActorsChanged.Broadcast();
}
