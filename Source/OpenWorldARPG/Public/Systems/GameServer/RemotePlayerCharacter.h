// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RemotePlayerCharacter.generated.h"

/**
 * 远程玩家实体 (Phase 1: 状态同步 + AOI)
 *
 * 由 RemotePlayerManagerSubsystem 依据服务器 AOI 广播生成/销毁。
 * 无 GAS / 无输入 / 无摄像机，仅负责向服务器权威位置平滑插值，
 * 表现层数据（骨骼网格）由蓝图子类配置。
 */
UCLASS()
class OPENWORLDARPG_API ARemotePlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ARemotePlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void Tick(float DeltaTime) override;

    // 服务器权威 player_id（AOI 广播的标识）
    void SetRemotePlayerId(int64 InPlayerId) { RemotePlayerId = InPlayerId; }
    int64 GetRemotePlayerId() const { return RemotePlayerId; }

    // 设置服务器权威目标位置/朝向，Tick 中平滑逼近
    void SetTargetTransform(const FVector& Location, float Yaw);

    // 立即吸附到目标（进入视野 / 位置校正时使用）
    void SnapTo(const FVector& Location, float Yaw);

protected:
    // 位置插值速度系数：越大逼近越快，过大会产生瞬移感
    UPROPERTY(EditDefaultsOnly, Category = "RemotePlayer|Interp")
    float InterpSpeed = 12.0f;

    // 朝向插值速度（度/秒）
    UPROPERTY(EditDefaultsOnly, Category = "RemotePlayer|Interp")
    float YawInterpSpeed = 360.0f;

    // 单帧最大位移（m），防插值在远距离瞬移时穿墙
    UPROPERTY(EditDefaultsOnly, Category = "RemotePlayer|Interp")
    float MaxStepPerFrame = 2.0f;

    // 目标位置/朝向（服务器权威）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RemotePlayer")
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RemotePlayer")
    float TargetYaw = 0.0f;

private:
    int64 RemotePlayerId = 0;
};
