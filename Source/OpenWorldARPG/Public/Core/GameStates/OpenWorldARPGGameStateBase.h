// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "OpenWorldARPGGameStateBase.generated.h"

class UServerPlayerDataManager;

/**
 * 项目 GameState 基类。
 * 挂载 UServerPlayerDataManager 组件，承载服务器侧玩家存档（本作单玩家、多角色配队）。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	AOpenWorldARPGGameStateBase();

	// --- 服务器侧数据管理 ---

	/** 获取服务器侧玩家数据管理器。仅服务器有效。 */
	UServerPlayerDataManager* GetServerPlayerDataManager() const { return ServerPlayerDataManager; }

protected:
	// --- 服务器侧组件 ---

	UPROPERTY(VisibleAnywhere, Category = "GameState|Server")
	TObjectPtr<UServerPlayerDataManager> ServerPlayerDataManager;
};
