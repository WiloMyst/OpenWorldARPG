// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameServer/RemotePlayerManagerSubsystem.h"
#include "Systems/GameServer/RemotePlayerCharacter.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "Engine/GameInstance.h"

void URemotePlayerManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameServerSubsystem* GameServer = GI->GetSubsystem<UGameServerSubsystem>())
            {
                GameServer->OnRemotePlayerEnter.AddDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerEnter);
                GameServer->OnRemotePlayerLeave.AddDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerLeave);
                GameServer->OnRemotePlayerMove.AddDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerMove);
            }
        }
    }
}

void URemotePlayerManagerSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UGameServerSubsystem* GameServer = GI->GetSubsystem<UGameServerSubsystem>())
            {
                GameServer->OnRemotePlayerEnter.RemoveDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerEnter);
                GameServer->OnRemotePlayerLeave.RemoveDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerLeave);
                GameServer->OnRemotePlayerMove.RemoveDynamic(this, &URemotePlayerManagerSubsystem::HandleRemotePlayerMove);
            }
        }
    }

    ClearAllRemotePlayers();
    Super::Deinitialize();
}

ARemotePlayerCharacter* URemotePlayerManagerSubsystem::GetRemotePlayer(int64 PlayerId) const
{
    if (const TObjectPtr<ARemotePlayerCharacter>* Found = RemotePlayers.Find(PlayerId))
    {
        return Found->Get();
    }
    return nullptr;
}

void URemotePlayerManagerSubsystem::ClearAllRemotePlayers()
{
    for (auto& Pair : RemotePlayers)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->Destroy();
        }
    }
    RemotePlayers.Reset();
}

void URemotePlayerManagerSubsystem::HandleRemotePlayerEnter(int64 PlayerId, const FString& Account, FVector Location, float Yaw)
{
    if (GetRemotePlayer(PlayerId))
    {
        // 重复进入（异常时序）：仅刷新目标位置
        GetRemotePlayer(PlayerId)->SetTargetTransform(Location, Yaw);
        return;
    }

    if (ARemotePlayerCharacter* Remote = SpawnRemotePlayer(PlayerId, Location, Yaw))
    {
        RemotePlayers.Add(PlayerId, Remote);
    }
}

void URemotePlayerManagerSubsystem::HandleRemotePlayerLeave(int64 PlayerId)
{
    if (ARemotePlayerCharacter* Remote = GetRemotePlayer(PlayerId))
    {
        Remote->Destroy();
    }
    RemotePlayers.Remove(PlayerId);
}

void URemotePlayerManagerSubsystem::HandleRemotePlayerMove(int64 PlayerId, FVector Location, float Yaw)
{
    if (ARemotePlayerCharacter* Remote = GetRemotePlayer(PlayerId))
    {
        Remote->SetTargetTransform(Location, Yaw);
    }
}

ARemotePlayerCharacter* URemotePlayerManagerSubsystem::SpawnRemotePlayer(int64 PlayerId, const FVector& Location, float Yaw)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    TSubclassOf<ARemotePlayerCharacter> ClassToSpawn = RemotePlayerClass
        ? RemotePlayerClass
        : TSubclassOf<ARemotePlayerCharacter>(ARemotePlayerCharacter::StaticClass());

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ARemotePlayerCharacter* Remote = World->SpawnActor<ARemotePlayerCharacter>(ClassToSpawn,
        FTransform(FRotator(0.0f, Yaw, 0.0f), Location), Params);
    if (Remote)
    {
        Remote->SetRemotePlayerId(PlayerId);
        Remote->SnapTo(Location, Yaw);
    }
    return Remote;
}
