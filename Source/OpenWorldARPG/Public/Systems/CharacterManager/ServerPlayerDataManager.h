// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "Systems/QuestSystem/Data/QuestTypes.h"
#include "ServerPlayerDataManager.generated.h"

class APlayerCharacter;

/**
 * 单个玩家的全部角色存档。用 USTRUCT 包装以支持 UPROPERTY TMap 反射。
 */
USTRUCT()
struct FPlayerSaveDataEntry
{
    GENERATED_BODY()

    /** 该玩家拥有的所有角色存档，按 CharacterTag 索引。 */
    UPROPERTY()
    TMap<FGameplayTag, FCharacterSaveData> CharacterSaveDataMap;

    /** 该玩家的任务进度存档，按 QuestTag 索引。 */
    UPROPERTY()
    TMap<FGameplayTag, FQuestProgress> QuestProgressMap;
};

/**
 * 服务器侧玩家数据管理器（挂载于 GameState）。
 *
 * 【职责】
 * 服务器权威持有每个登录玩家的角色存档数据，按 UniqueNetId 索引。
 * GameMode 在 PostLogin / 编队变更时通过本组件读取目标玩家的存档来 Spawn 角色。
 *
 * 【为什么不用 UGameInstanceSubsystem】
 * GameInstanceSubsystem 在一个 GameInstance 上只有一个实例，若直接持有 per-玩家数据，
 * 多个玩家的存档会串。本组件用 TMap<FString, FPlayerSaveDataEntry> 显式按玩家 ID 隔离。
 *
 * 【为什么不用 ULocalPlayerSubsystem】
 * Dedicated Server 上没有 ULocalPlayer，ULocalPlayerSubsystem 根本不会创建，
 * 服务器侧无法通过它读取存档数据。
 *
 * 【数据来源（当前：模拟）】
 * 当前项目无真实后端数据库，登录时从 UOpenWorldARPGSettings 配置的 UInitialArchiveData
 * 加载一份模拟存档给所有登录玩家。
 * 【数据来源（接入真实数据库后）】
 * OnPlayerConnected 应改为按 PlayerState->GetUniqueId() 异步向后端服务请求该玩家的存档，
 * 请求返回后再触发 GameMode 的角色生成流程。具体见 .cpp 中标注的 [DB-INTEGRATION] 注释。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UServerPlayerDataManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UServerPlayerDataManager();

    // --- 玩家存档生命周期 ---

    /**
     * 玩家登录时调用。服务器权威加载该玩家的角色存档。
     * [DB-INTEGRATION] 接入真实数据库后，本函数应改为异步：
     *   1. 向后端服务发起异步请求（按 PlayerState->GetUniqueId()）
     *   2. 请求返回前挂起 GameMode 的角色生成流程
     *   3. 数据到达后回调通知 GameMode 继续生成
     * 当前实现：从 UInitialArchiveData 同步加载一份模拟存档给该玩家。
     */
    void OnPlayerConnected(APlayerController* PlayerController);

    /** 玩家离线时调用，持久化并清理内存。 */
    void OnPlayerDisconnected(APlayerController* PlayerController);

    // --- 存档查询（服务器侧权威） ---

    /** 获取指定玩家拥有的所有角色存档。服务器专用。 */
    bool GetAllOwnedCharacterSaveData(APlayerController* PlayerController, TArray<FCharacterSaveData>& OutData) const;

    /** 获取指定玩家某个角色的存档。服务器专用。 */
    const FCharacterSaveData* GetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag) const;

    /** 更新指定玩家某个角色的存档。服务器专用。 */
    void SetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);

    /** 从角色实体收集运行时数据并回写到玩家存档。服务器专用。 */
    void CollectSaveDataFromCharacters(APlayerController* PlayerController, const TArray<APlayerCharacter*>& CharacterActors);

    // --- 任务存档（服务器侧权威） ---

    /** 获取指定玩家的任务进度存档。服务器专用。 */
    const TMap<FGameplayTag, FQuestProgress>* GetQuestProgress(APlayerController* PlayerController) const;

    /** 更新指定玩家的任务进度存档。服务器专用。 */
    void SetQuestProgress(APlayerController* PlayerController, const TMap<FGameplayTag, FQuestProgress>& InProgress);

private:
    /** 按 UniqueNetId 索引的 per-玩家存档。 */
    UPROPERTY()
    TMap<FString, FPlayerSaveDataEntry> PlayerSaveDataMap;

    /** UniqueNetId → PlayerController 反向索引（便于按 PC 查询）。 */
    UPROPERTY()
    TMap<FString, TWeakObjectPtr<APlayerController>> PlayerControllerMap;
};
