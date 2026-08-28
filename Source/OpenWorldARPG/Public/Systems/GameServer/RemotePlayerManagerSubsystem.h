// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RemotePlayerManagerSubsystem.generated.h"

class ARemotePlayerCharacter;

/**
 * 远程玩家管理器 (Phase 1: 状态同步 + AOI)
 *
 * 订阅 UGameServerSubsystem 的 AOI 广播事件，维护 player_id -> 远程玩家实体 的映射：
 * - PlayerEnter  -> 生成远程玩家实体
 * - PlayerLeave  -> 销毁远程玩家实体
 * - PlayerMove   -> 更新远程玩家目标位置（实体内部平滑插值）
 */
UCLASS()
class OPENWORLDARPG_API URemotePlayerManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 远程玩家生成类；默认使用 C++ 基类，蓝图子类可覆盖外观
    UPROPERTY(EditDefaultsOnly, Category = "RemotePlayer")
    TSubclassOf<ARemotePlayerCharacter> RemotePlayerClass;

    // 查询远程玩家实体；不存在返回 nullptr
    ARemotePlayerCharacter* GetRemotePlayer(int64 PlayerId) const;

    // 当前视野内远程玩家数量
    int32 GetRemotePlayerCount() const { return RemotePlayers.Num(); }

    // 清空所有远程玩家（登出/断线时调用）
    void ClearAllRemotePlayers();

private:
    UFUNCTION()
    void HandleRemotePlayerEnter(int64 PlayerId, const FString& Account, FVector Location, float Yaw);

    UFUNCTION()
    void HandleRemotePlayerLeave(int64 PlayerId);

    UFUNCTION()
    void HandleRemotePlayerMove(int64 PlayerId, FVector Location, float Yaw);

    ARemotePlayerCharacter* SpawnRemotePlayer(int64 PlayerId, const FVector& Location, float Yaw);

    UPROPERTY()
    TMap<int64, TObjectPtr<ARemotePlayerCharacter>> RemotePlayers;
};
