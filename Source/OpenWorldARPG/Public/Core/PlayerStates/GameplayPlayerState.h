// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGPlayerState.h"
#include "GameplayTagContainer.h"
#include "GameplayPlayerState.generated.h"

class APlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterIndexChanged, int32, OldIndex, int32, NewIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamCharacterActorsChanged);

/**
 * 通用玩法 PlayerState。掌管队伍角色实体的网络同步数据。
 * 服务器是数据真理，客户端通过 Replicated 属性接收同步。
 */
UCLASS()
class OPENWORLDARPG_API AGameplayPlayerState : public AOpenWorldARPGPlayerState
{
	GENERATED_BODY()

public:
	AGameplayPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 队伍角色管理 (服务器权威) ---

	const TArray<APlayerCharacter*>& GetTeamCharacterActors() const { return TeamCharacterActors; }
	APlayerCharacter* GetTeamCharacterByIndex(int32 Index) const;
	APlayerCharacter* GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const;
	void GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const;
	void SetTeamCharacterActors(const TArray<APlayerCharacter*>& InActors);
	void AddTeamCharacter(APlayerCharacter* InCharacter);

	// --- 激活角色索引 (服务器权威 + OnRep) ---

	int32 GetActiveCharacterIndex() const { return ActiveCharacterIndex; }
	APlayerCharacter* GetActiveCharacter() const { return GetTeamCharacterByIndex(ActiveCharacterIndex); }
	void SetActiveCharacterIndex(int32 NewIndex);

protected:
	// --- OnRep 回调 ---

	UFUNCTION()
	void OnRep_ActiveCharacterIndex(int32 OldIndex);

	UFUNCTION()
	void OnRep_TeamCharacterActors();

public:
	// --- 事件委托 ---

	UPROPERTY(BlueprintAssignable, Category = "PlayerState|Events")
	FOnActiveCharacterIndexChanged OnActiveCharacterIndexChanged;

	UPROPERTY(BlueprintAssignable, Category = "PlayerState|Events")
	FOnTeamCharacterActorsChanged OnTeamCharacterActorsChanged;

private:
	// --- 网络同步数据 ---

	UPROPERTY(ReplicatedUsing = OnRep_TeamCharacterActors)
	TArray<APlayerCharacter*> TeamCharacterActors;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveCharacterIndex)
	int32 ActiveCharacterIndex = -1;
};
