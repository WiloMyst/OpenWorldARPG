// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "ServerPlayerDataManager.generated.h"

class APlayerCharacter;

/**
 * 玩家的全部角色存档（本作单玩家，队伍内含多角色）。
 * 用 USTRUCT 包装以支持 UPROPERTY 反射。
 */
USTRUCT()
struct FPlayerSaveDataEntry
{
    GENERATED_BODY()

    /** 该玩家拥有的所有角色存档，按 CharacterTag 索引。 */
    UPROPERTY()
    TMap<FGameplayTag, FCharacterSaveData> CharacterSaveDataMap;
};

/**
 * 服务器侧玩家数据管理器（挂载于 GameState）。
 *
 * 【职责】
 * 服务器权威持有玩家（本作唯一玩家）的角色存档数据，作为多角色配队的存档底座。
 * GameMode 在 PostLogin / 编队变更时通过本组件读取存档来 Spawn 角色。
 *
 * 【单玩家设计】
 * 本作不存在多玩家并发登录：一台 Dedicated Server 上只有唯一玩家、多个上阵角色，
 * 因此不再按 UniqueNetId 建立 per-玩家隔离索引，直接持有单份 { CharacterTag -> SaveData }
 * 队伍存档（FPlayerSaveDataEntry）。方法签名保留 APlayerController* 仅作服务端上下文，
 * 不参与索引。若未来演进为多玩家，再改回按玩家 ID 分桶即可。
 *
 * 【为什么不用 UGameInstanceSubsystem / ULocalPlayerSubsystem】
 * GameInstanceSubsystem 在服务器侧跨关卡常驻，但职责聚焦单玩家存档更利于测试与独立挂载；
 * ULocalPlayerSubsystem 在 Dedicated Server 上不存在，无法承载服务器侧存档。
 * 故沿用 GameState 组件挂载方案。
 *
 * 【数据来源（当前：模拟）】
 * 当前项目无真实后端数据库，登录时从 UOpenWorldARPGSettings 配置的 UInitialArchiveData
 * 加载一份模拟存档。
 * 【数据来源（接入真实数据库后）】
 * OnPlayerConnected 应改为异步向后端服务请求玩家存档，请求返回后再触发 GameMode 的角色生成流程。
 * 具体见 .cpp 中标注的 [DB-INTEGRATION] 注释。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UServerPlayerDataManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UServerPlayerDataManager();

    // --- 玩家存档生命周期 ---

    /**
     * 玩家登录时调用。服务器权威加载玩家的角色存档（单玩家）。
     * [DB-INTEGRATION] 接入真实数据库后，本函数应改为异步：
     *   1. 向后端服务发起异步请求（按 PlayerState->GetUniqueId()）
     *   2. 请求返回前挂起 GameMode 的角色生成流程
     *   3. 数据到达后回调通知 GameMode 继续生成
     * 当前实现：从 UInitialArchiveData 同步加载一份模拟存档。
     */
    void OnPlayerConnected(APlayerController* PlayerController);

    /** 玩家离线时调用，持久化并清理内存。 */
    void OnPlayerDisconnected(APlayerController* PlayerController);

    // --- 存档查询（服务器侧权威） ---

    /** 获取玩家拥有的所有角色存档。服务器专用。 */
    bool GetAllOwnedCharacterSaveData(APlayerController* PlayerController, TArray<FCharacterSaveData>& OutData) const;

    /** 获取玩家某个角色的存档。服务器专用。 */
    const FCharacterSaveData* GetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag) const;

    /** 更新玩家某个角色的存档。服务器专用。 */
    void SetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData);

    /** 从角色实体收集运行时数据并回写到玩家存档。服务器专用。 */
    void CollectSaveDataFromCharacters(APlayerController* PlayerController, const TArray<APlayerCharacter*>& CharacterActors);

private:
    /** 单玩家队伍角色存档：按 CharacterTag 索引（无 per-玩家维度）。 */
    UPROPERTY()
    FPlayerSaveDataEntry OwnedSaveData;
};
