// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/ServerPlayerDataManager.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "Core/Data/InitialArchiveData.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

// 辅助：判断当前组件所在是否为服务器权威
static bool HasServerAuthority(const UActorComponent* Comp)
{
    return Comp && Comp->GetOwnerRole() == ROLE_Authority;
}

// 单玩家判定：本作只有唯一玩家，PC 仅作服务端上下文，不参与存档索引。
static bool IsServerContext(const UActorComponent* Comp, APlayerController* PC)
{
    return HasServerAuthority(Comp) && PC != nullptr;
}

UServerPlayerDataManager::UServerPlayerDataManager()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false); // 纯服务器侧数据，不复制
}

void UServerPlayerDataManager::OnPlayerConnected(APlayerController* PlayerController)
{
    if (!IsServerContext(this, PlayerController)) return;

    // [DB-INTEGRATION] 接入真实数据库后，此处应改为异步：
    //   BackendService->RequestPlayerSaveData(
    //       FOnSaveDataReceived::CreateWeakLambda(this, [this, PlayerController](const TArray<FCharacterSaveData>& Data)
    //       {
    //           for (const FCharacterSaveData& Item : Data)
    //           {
    //               if (Item.CharacterTag.IsValid())
    //               {
    //                   OwnedSaveData.CharacterSaveDataMap.Add(Item.CharacterTag, Item);
    //               }
    //           }
    //           OnPlayerDataReady.Broadcast(PlayerController); // 通知 GameMode 继续生成角色
    //       }));
    //   2. 在数据到达前，GameMode 的 PostLogin 应挂起，等待 OnPlayerDataReady 回调
    //   3. 失败重试、超时、断线重连等逻辑也在此处处理
    //
    // 当前实现：从 UInitialArchiveData 同步加载一份模拟存档（单玩家共享）。

    if (OwnedSaveData.CharacterSaveDataMap.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerConnected: 玩家存档已初始化，跳过。"));
        return;
    }

    // 服务器权威存档优先 (B 方案): 登录响应已携带拥有角色 (细节字段已由 GameServerSubsystem
    // 用本地资产兜底合并), 直接采用, 不再从本地 UInitialArchiveData 模拟加载.
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameServerSubsystem* GameServer = GI->GetSubsystem<UGameServerSubsystem>())
            {
                if (GameServer->HasServerArchive())
                {
                    for (const FCharacterSaveData& SaveData : GameServer->GetServerOwnedCharacters())
                    {
                        if (SaveData.CharacterTag.IsValid())
                        {
                            OwnedSaveData.CharacterSaveDataMap.Add(SaveData.CharacterTag, SaveData);
                        }
                    }

                    UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerConnected: 采用服务器权威存档，角色数=%d"),
                        OwnedSaveData.CharacterSaveDataMap.Num());
                    return;
                }
            }
        }
    }

    // 兜底: 未登录/无服务器存档时, 从项目设置加载初始存档（模拟数据库读取）
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

    // 将初始拥有角色数据填入单玩家队伍存档（按 CharacterTag 索引）
    for (const FCharacterSaveData& SaveData : ArchiveData->InitialOwnedCharacters)
    {
        if (SaveData.CharacterTag.IsValid())
        {
            OwnedSaveData.CharacterSaveDataMap.Add(SaveData.CharacterTag, SaveData);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerConnected: 玩家存档初始化完成，角色数=%d"),
        OwnedSaveData.CharacterSaveDataMap.Num());
}

void UServerPlayerDataManager::OnPlayerDisconnected(APlayerController* PlayerController)
{
    if (!PlayerController) return;

    // [DB-INTEGRATION] 接入真实数据库后，此处应先异步写回存档：
    //   BackendService->SavePlayerData(OwnedSaveData.CharacterSaveDataMap,
    //       FOnSaveComplete::CreateWeakLambda(this, [this](bool bSuccess)
    //       {
    //           if (bSuccess) { OwnedSaveData.CharacterSaveDataMap.Empty(); }
    //           else { UE_LOG(...); /* 重试或保留内存等待下次保存 */ }
    //       }));
    // 当前实现：直接从内存清理。

    OwnedSaveData.CharacterSaveDataMap.Empty();
    UE_LOG(LogTemp, Log, TEXT("[ServerPlayerData] OnPlayerDisconnected: 玩家存档已从内存清理。"));
}

bool UServerPlayerDataManager::GetAllOwnedCharacterSaveData(APlayerController* PlayerController, TArray<FCharacterSaveData>& OutData) const
{
    if (!IsServerContext(this, PlayerController)) return false;

    OwnedSaveData.CharacterSaveDataMap.GenerateValueArray(OutData);
    return OutData.Num() > 0;
}

const FCharacterSaveData* UServerPlayerDataManager::GetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag) const
{
    if (!IsServerContext(this, PlayerController)) return nullptr;

    return OwnedSaveData.CharacterSaveDataMap.Find(CharacterTag);
}

void UServerPlayerDataManager::SetCharacterSaveData(APlayerController* PlayerController, const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData)
{
    if (!IsServerContext(this, PlayerController)) return;

    FCharacterSaveData* ExistingData = OwnedSaveData.CharacterSaveDataMap.Find(CharacterTag);
    if (ExistingData)
    {
        *ExistingData = NewData;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ServerPlayerData] SetCharacterSaveData: 角色 Tag [%s] 不在玩家存档中。"),
            *CharacterTag.ToString());
    }
}

void UServerPlayerDataManager::CollectSaveDataFromCharacters(APlayerController* PlayerController, const TArray<APlayerCharacter*>& CharacterActors)
{
    if (!IsServerContext(this, PlayerController)) return;

    // 从角色实体收集运行时数据并回写到对应存档
    for (APlayerCharacter* Character : CharacterActors)
    {
        if (!IsValid(Character)) continue;

        const FCharacterSaveData& RuntimeData = Character->GetRuntimeData();
        FCharacterSaveData* Data = OwnedSaveData.CharacterSaveDataMap.Find(RuntimeData.CharacterTag);
        if (Data)
        {
            *Data = RuntimeData;
        }
    }

    // [DB-INTEGRATION] 接入真实数据库后，此处应触发异步写回：
    //   BackendService->SavePlayerData(OwnedSaveData.CharacterSaveDataMap);
    // 当前实现：数据仅在内存中，不持久化。
}