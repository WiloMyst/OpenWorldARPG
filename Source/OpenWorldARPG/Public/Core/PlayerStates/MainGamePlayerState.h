// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "MainGamePlayerState.generated.h"

class APlayerCharacter;

// --- 委托 ---

/** 当激活角色索引变化时广播（参数：旧索引, 新索引） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterIndexChanged, int32, OldIndex, int32, NewIndex);

/** 当队伍角色列表变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamCharacterActorsChanged);

/**
 * 主游戏 PlayerState。掌管队伍角色实体的网络同步数据。
 * 服务器是数据真理，客户端通过 Replicated 属性接收同步。
 */
UCLASS()
class OPENWORLDARPG_API AMainGamePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMainGamePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 队伍角色管理 (服务器权威) ---

	/** 获取队伍角色实例数组 */
	UFUNCTION(BlueprintPure, Category = "PlayerState|Team")
	const TArray<APlayerCharacter*>& GetTeamCharacterActors() const { return TeamCharacterActors; }

	/** 根据队伍索引获取角色实例 */
	UFUNCTION(BlueprintPure, Category = "PlayerState|Team")
	APlayerCharacter* GetTeamCharacterByIndex(int32 Index) const;

	/** 根据角色 Tag 获取角色实例 */
	UFUNCTION(BlueprintPure, Category = "PlayerState|Team")
	APlayerCharacter* GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const;

	/** 获取所有队伍角色实例（过滤无效引用） */
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Team")
	void GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const;

	/** 服务器专用：设置队伍角色数组 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Team")
	void SetTeamCharacterActors(const TArray<APlayerCharacter*>& InActors);

	/** 服务器专用：添加一个角色到队伍数组 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Team")
	void AddTeamCharacter(APlayerCharacter* InCharacter);

	// --- 激活角色索引 (服务器权威 + OnRep) ---

	/** 获取当前激活角色索引 */
	UFUNCTION(BlueprintPure, Category = "PlayerState|Team")
	int32 GetActiveCharacterIndex() const { return ActiveCharacterIndex; }

	/** 获取当前激活角色实例 */
	UFUNCTION(BlueprintPure, Category = "PlayerState|Team")
	APlayerCharacter* GetActiveCharacter() const;

	/** 服务器专用：设置激活角色索引（会触发全网同步） */
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Team")
	void SetActiveCharacterIndex(int32 NewIndex);

	// --- 事件 (OnRep 驱动) ---

	UPROPERTY(BlueprintAssignable, Category = "PlayerState|Events")
	FOnActiveCharacterIndexChanged OnActiveCharacterIndexChanged;

	UPROPERTY(BlueprintAssignable, Category = "PlayerState|Events")
	FOnTeamCharacterActorsChanged OnTeamCharacterActorsChanged;

protected:
	UFUNCTION()
	void OnRep_ActiveCharacterIndex(int32 OldIndex);

	UFUNCTION()
	void OnRep_TeamCharacterActors();

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamCharacterActors)
	TArray<APlayerCharacter*> TeamCharacterActors;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveCharacterIndex)
	int32 ActiveCharacterIndex = -1;
};
