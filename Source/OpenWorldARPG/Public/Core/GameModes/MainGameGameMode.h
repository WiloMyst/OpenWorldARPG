// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "MainGameGameMode.generated.h"

class APlayerCharacter;

/**
 * @class AMainGameGameMode
 * @brief 主游戏 GameMode，管理关卡内的角色 Actor 引用。
 * 
 * 架构原则：Actor 引用属于 World 层，由 GameMode 管理。
 * 关卡卸载时 GameMode 自动销毁，Actor 引用随之消失，不存在悬空指针问题。
 */
UCLASS()
class OPENWORLDARPG_API AMainGameGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMainGameGameMode();

    // ---- 角色 Actor 管理 (World 层) ----

    /** 根据角色Tag获取队伍中的角色实例 */
    UFUNCTION(BlueprintCallable, Category = "MainGameGameMode")
    APlayerCharacter* GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const;

    /** 获取所有已拥有的角色实例 */
    UFUNCTION(BlueprintCallable, Category = "MainGameGameMode")
    void GetAllOwnedCharacters(TArray<APlayerCharacter*>& OutCharacters) const;

    /** 获取所有队伍角色实例 */
    UFUNCTION(BlueprintCallable, Category = "MainGameGameMode")
    void GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const;

protected:
    virtual void BeginPlay() override;

    // ==========================================
    // 核心初始化流程
    // ==========================================

    void GeneratePlayerCharacters();

    // 延时清理资产
    void CleanupAfterLoad();

protected:
    // ==========================================
    // 暴露给蓝图的配置项 (彻底告别硬编码)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    FName DefaultPlayerStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    TSubclassOf<APlayerCharacter> PlayerCharacterClass;

    UPROPERTY(EditDefaultsOnly, Category = "GameMode|Config")
    float AssetCleanupDelay;

    // ==========================================
    // 角色 Actor 引用 (随关卡销毁自然清理)
    // ==========================================

    /** 所有已拥有的角色实例 (包含队伍内和队伍外) */
    UPROPERTY()
    TMap<FGameplayTag, APlayerCharacter*> OwnedCharacters;

    /** 当前出战队伍的角色实例 */
    UPROPERTY()
    TMap<FGameplayTag, APlayerCharacter*> TeamCharacterActors;

private:
    FTimerHandle CleanupTimerHandle;
};