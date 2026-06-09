// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerStates/MainGamePlayerState.h"
#include "Characters/PlayerCharacter.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Net/UnrealNetwork.h"

AMainGamePlayerState::AMainGamePlayerState()
{
}

void AMainGamePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 队伍角色数组仅同步给拥有此 PlayerState 的客户端
	// 其他客户端不需要也不应持有别人的非激活角色引用
	DOREPLIFETIME_CONDITION(AMainGamePlayerState, TeamCharacterActors, COND_OwnerOnly);
	DOREPLIFETIME(AMainGamePlayerState, ActiveCharacterIndex);
}

// --- 队伍角色 Actor 管理 ---

APlayerCharacter* AMainGamePlayerState::GetTeamCharacterByIndex(int32 Index) const
{
	if (TeamCharacterActors.IsValidIndex(Index))
	{
		APlayerCharacter* Character = TeamCharacterActors[Index];
		return IsValid(Character) ? Character : nullptr;
	}
	return nullptr;
}

APlayerCharacter* AMainGamePlayerState::GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const
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

void AMainGamePlayerState::GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const
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

void AMainGamePlayerState::SetTeamCharacterActors(const TArray<APlayerCharacter*>& InActors)
{
	if (HasAuthority())
	{
		TeamCharacterActors = InActors;
	}
}

void AMainGamePlayerState::AddTeamCharacter(APlayerCharacter* InCharacter)
{
	if (HasAuthority() && IsValid(InCharacter))
	{
		TeamCharacterActors.Add(InCharacter);
	}
}

// --- 激活角色索引 ---

APlayerCharacter* AMainGamePlayerState::GetActiveCharacter() const
{
	return GetTeamCharacterByIndex(ActiveCharacterIndex);
}

void AMainGamePlayerState::SetActiveCharacterIndex(int32 NewIndex)
{
	if (HasAuthority())
	{
		int32 OldIndex = ActiveCharacterIndex;
		ActiveCharacterIndex = NewIndex;

		// 服务器本地立即触发事件（OnRep 只在客户端触发）
		if (OldIndex != NewIndex)
		{
			OnActiveCharacterIndexChanged.Broadcast(OldIndex, NewIndex);
		}
	}
}

// --- OnRep 回调 (客户端收到同步后，驱动本地 UI) ---

void AMainGamePlayerState::OnRep_ActiveCharacterIndex(int32 OldIndex)
{
	// 客户端收到 ActiveCharacterIndex 同步后：
	// 1. 广播本地事件，驱动 UI 刷新（使用 UE 提供的旧值参数）
	OnActiveCharacterIndexChanged.Broadcast(OldIndex, ActiveCharacterIndex);

	// 2. 通知 TeamManagerSubsystem 更新本地缓存
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>())
		{
			TeamManager->OnRep_ActiveCharacterIndexFromServer(ActiveCharacterIndex);
		}
	}
}

void AMainGamePlayerState::OnRep_TeamCharacterActors()
{
	// 客户端收到队伍角色列表同步后：
	// 1. 广播本地事件
	OnTeamCharacterActorsChanged.Broadcast();

	// 2. 通知 TeamManagerSubsystem 更新本地缓存
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>())
		{
			TeamManager->OnRep_TeamCharacterActorsFromServer(TeamCharacterActors);
		}
	}
}
