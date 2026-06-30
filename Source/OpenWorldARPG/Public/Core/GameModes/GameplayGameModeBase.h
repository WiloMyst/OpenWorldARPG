// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "GameplayTagContainer.h"
#include "GameplayGameModeBase.generated.h"

class AGameplayPlayerState;
class AGameplayPlayerController;

/**
 * 通用玩法 GameMode。负责服务器端角色生成与初始化。
 * 无论在大世界还是副本，角色实体引用存入 PlayerState（Replicated），GameMode 不保留角色 TMap。
 * 大世界/副本专属逻辑在子类中扩展。
 *
 * 【职责边界】
 * GameMode 是世界实体的唯一工厂与权威：
 * - 所有 APlayerCharacter 的 Spawn / Destroy 必须在此层完成。
 * - PlayerController 仅作为 RPC 邮局转发请求，不直接 Spawn/Destroy。
 */
UCLASS()
class OPENWORLDARPG_API AGameplayGameModeBase : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    AGameplayGameModeBase();

    // --- 生命周期 ---

    virtual void PostLogin(APlayerController* NewPlayer) override;

    /**
     * 处理玩家队伍的实体替换与生成（服务器权威）。
     * 销毁旧队伍 → 按 NewTeamTags Spawn 新角色 → 写入 PlayerState → Possess 激活角色。
     *
     * @param PlayerController 发起请求的玩家控制器
     * @param NewTeamTags 新队伍角色 Tag 数组（不含空 Tag）
     * @param ActiveIndex 新队伍中要激活的角色索引
     */
    void ApplyPlayerTeamChanges(AGameplayPlayerController* PlayerController, const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex);

protected:
    // --- 初始化流程 ---
    void GeneratePlayerCharacters(APlayerController* PlayerController);
    void CleanupAfterLoad();

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    FName DefaultPlayerStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    float AssetCleanupDelay;

private:
    FTimerHandle CleanupTimerHandle;
};
