// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "MainGameGameMode.generated.h"

class APlayerCharacter;
class AMainGamePlayerState;

/**
 * @class AMainGameGameMode
 * @brief 主游戏 GameMode，负责服务器端角色生成与初始化。
 *
 * 架构原则（Client-Server Authoritative Model）：
 * - GameMode 只在服务器存在，负责生成角色 Actor
 * - 角色实体引用存入 PlayerState（Replicated），不再存入 GameMode
 * - 角色生成时机从 BeginPlay 迁移到 PostLogin（按玩家连接触发）
 * - GameMode 不保留任何角色 TMap，所有查询通过 PlayerState 进行
 */
UCLASS()
class OPENWORLDARPG_API AMainGameGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMainGameGameMode();

    // ---- 生命周期 ----

    /** 重写 PostLogin：玩家加入后为其生成角色实体 */
    virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
    // ==========================================
    // 核心初始化流程
    // ==========================================

    /** 为指定玩家生成所有拥有的角色实体 */
    void GeneratePlayerCharacters(APlayerController* PlayerController);

    /** 延时清理资产 */
    void CleanupAfterLoad();

protected:
    // ==========================================
    // 暴露给蓝图的配置项
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    FName DefaultPlayerStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    TSubclassOf<APlayerCharacter> PlayerCharacterClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    float AssetCleanupDelay;

private:
    FTimerHandle CleanupTimerHandle;
};
