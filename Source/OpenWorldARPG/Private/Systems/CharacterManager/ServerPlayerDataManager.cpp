// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/ServerPlayerDataManager.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "Core/Data/InitialArchiveData.h"
#include "Core/OpenWorldARPGSettings.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

// 辅助：判断当前组件所在是否为服务器权威
static bool HasServerAuthority(const UActorComponent* Comp)
{
    return Comp && Comp->GetOwnerRole() == ROLE_Authority;
}

// 辅助：从 PlayerController 获取 per-玩家索引键
// 用 PlayerName 作为 key（避免引入 OnlineSubsystem 模块依赖）
// [DB-INTEGRATION] 接入真实数据库后应改用 UniqueNetId 索引
static FString GetPlayerKey(APlayerController* PC)
{
    if (APlayerState* PS = PC ? PC->GetPlayerState<APlayerState>() : nullptr)
    {
        return PS->GetPlayerName();
    }
    return TEXT("LocalPlayer_Fallback");
}

UServerPlayerDataManager::UServerPlayerDataManager()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false); // 纯服务器侧数据，不复制
}

void UServerPlayerDataManager::OnPlayerConnected(APlayerController* PlayerController)
{
    if (!HasServerAuthority(this) || !PlayerController) return;

    // 用 UniqueNetId 字符串作为 per-玩家索引键
    const FString PlayerKey = GetPlayerKey(PlayerController);
    if (PlayerKey == TEXT("LocalPlayer_Fallback"))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ServerPlayerData] OnPlayerConnected: PlayerController 无有效 UniqueNetId，使用 fallback 键。"));
    }

    // [DB-INTEGRATION] 接入真实数据库后，此处应改为：
    //   1. 调用异步后端请求：BackendService->RequestPlayerSaveData(UniqueId,
    //        FOnSaveDataReceived::CreateWeakLambda(this, [this, PlayerController](const TArray<FCharacterSaveData>& Data)
    //        {
    //            TMap<FGameplayTag, FCharacterSaveData>& PlayerData = PlayerSaveDataMap.Add(UniqueIdStr);
    //            for (const FCharacterSaveData& Item : Data) { PlayerData.Add(Item.CharacterTag, Item); }
    //            PlayerControllerMap.Add(UniqueIdStr, PlayerController);
    //            OnPlayerDataReady.Broadcast(PlayerController); // 通知 GameMode 继续生成角色
    //        }));
    //   2. 在数据到达前，GameMode 的 PostLogin 应挂起，等待 OnPlayerDataReady 回调
    //   3. 失败重试、超时、断线重连等逻辑也在此处处理
    //
    // 当前实现：从 UInitialArchiveData 同步加载一份模拟存档给该玩家。
    // 这份存档对所有玩家相同（因为来自项目设置），仅用于单机/测试场景。

    if (PlayerSaveDataMap.Contains(PlayerKey))
    {
        UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerConnected: 玩家 %s 已有存档，跳过初始化。"), *PlayerKey);
        return;
    }

    // 从项目设置加载初始存档（模拟数据库读取）
    const TSoftObjectPtr<UInitialArchiveData>& ArchiveSoftPtr = UOpenWorldARPGSettings::Get().InitialArchiveData;
    if (ArchiveSoftPtr.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("[ServerPlayerData] OpenWorldARPGSettings.InitialArchiveData 未配置！无法初始化玩家存档。"));
        return;
    }

    UInitialArchiveData* ArchiveData = ArchiveSoftPtr.LoadSynchronous();
    if (!ArchiveData)
    {
        UE_LOG(LogTemp, Error, TEXT("[ServerPlayerData] InitialArchiveData 同步加载失败！"));
        return;
    }

    // 将初始拥有角色数据存入 per-玩家 Map（以 UniqueNetId 为键）
    FPlayerSaveDataEntry& PlayerData = PlayerSaveDataMap.Add(PlayerKey);
    for (const FCharacterSaveData& SaveData : ArchiveData->InitialOwnedCharacters)
    {
        if (SaveData.CharacterTag.IsValid())
        {
            PlayerData.CharacterSaveDataMap.Add(SaveData.CharacterTag, SaveData);
        }
    }

    PlayerControllerMap.Add(PlayerKey, PlayerController);

    UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerConnected: 玩家 %s 存档初始化完成，角色数=%d"),
        *PlayerKey, PlayerData.CharacterSaveDataMap.Num());
}

void UServerPlayerDataManager::OnPlayerDisconnected(APlayerController* PlayerController)
{
    if (!PlayerController) return;

    const FString PlayerKey = GetPlayerKey(PlayerController);

    // [DB-INTEGRATION] 接入真实数据库后，此处应先异步写回该玩家的存档：
    //   BackendService->SavePlayerData(UniqueId, PlayerSaveDataMap[PlayerKey],
    //       FOnSaveComplete::CreateWeakLambda(this, [this, PlayerKey](bool bSuccess)
    //       {
    //           if (bSuccess) { PlayerSaveDataMap.Remove(PlayerKey); PlayerControllerMap.Remove(PlayerKey); }
    //           else { UE_LOG(...); /* 重试或保留内存等待下次保存 */ }
    //       }));
    // 当前实现：直接从内存清理。

    if (PlayerSaveDataMap.Remove(PlayerKey) > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerDisconnected: 玩家 %s 存档已从内存清理。"), *PlayerKey);
    }
    PlayerControllerMap.Remove(PlayerKey);
}

bool UServerPlayerDataManager::GetAllOwnedCharacterSaveData(APlayerController* PlayerController, TArray<FCharacterSaveData>& OutData) const
{
    if (!HasServerAuthority(this) || !PlayerController) return false;

    const FString PlayerKey = GetPlayerKey(PlayerController);

    const FPlayerSaveDataEntry* Found = PlayerSaveDataMap.Find(PlayerKey);
    if (!Found) return false;

    Found->CharacterSaveDataMap.GenerateValueArray(OutData);
    return true;
}

const FCharacterSaveData* UServerPlayerDataManager::GetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag) const
{
    if (!HasServerAuthority(this) || !PlayerController) return nullptr;

    const FString PlayerKey = GetPlayerKey(PlayerController);

    const FPlayerSaveDataEntry* Found = PlayerSaveDataMap.Find(PlayerKey);
    if (!Found) return nullptr;

    return Found->CharacterSaveDataMap.Find(CharacterTag);
}

void UServerPlayerDataManager::SetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData)
{
    if (!HasServerAuthority(this) || !PlayerController) return;

    const FString PlayerKey = GetPlayerKey(PlayerController);

    FPlayerSaveDataEntry* Found = PlayerSaveDataMap.Find(PlayerKey);
    if (!Found)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ServerPlayerData] SetCharacterSaveData: 玩家 %s 无存档记录，跳过更新。"), *PlayerKey);
        return;
    }

    FCharacterSaveData* ExistingData = Found->CharacterSaveDataMap.Find(CharacterTag);
    if (ExistingData)
    {
        *ExistingData = NewData;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ServerPlayerData] SetCharacterSaveData: 角色 Tag [%s] 不在玩家 %s 的存档中。"),
            *CharacterTag.ToString(), *PlayerKey);
    }
}

void UServerPlayerDataManager::CollectSaveDataFromCharacters(APlayerController* PlayerController, const TArray<APlayerCharacter*>& CharacterActors)
{
    if (!HasServerAuthority(this) || !PlayerController) return;

    const FString PlayerKey = GetPlayerKey(PlayerController);

    FPlayerSaveDataEntry* Found = PlayerSaveDataMap.Find(PlayerKey);
    if (!Found) return;

    // 从角色实体收集运行时数据并回写到对应存档
    for (APlayerCharacter* Character : CharacterActors)
    {
        if (!IsValid(Character)) continue;

        const FCharacterSaveData& RuntimeData = Character->GetRuntimeData();
        FCharacterSaveData* Data = Found->CharacterSaveDataMap.Find(RuntimeData.CharacterTag);
        if (Data)
        {
            *Data = RuntimeData;
        }
    }

    // [DB-INTEGRATION] 接入真实数据库后，此处应触发异步写回：
    //   BackendService->SavePlayerData(UniqueId, Found->CharacterSaveDataMap);
    // 当前实现：数据仅在内存中，不持久化。
}

