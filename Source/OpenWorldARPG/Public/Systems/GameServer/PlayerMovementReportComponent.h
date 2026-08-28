// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerMovementReportComponent.generated.h"

class UGameServerSubsystem;

/**
 * 本地玩家移动上报组件 (Phase 1: 状态同步)
 *
 * 挂载于 APlayerCharacter，仅对本地控制角色生效：
 * 以固定频率将位置/朝向上报给 GameServerSubsystem（服务器权威校验后广播），
 * 并监听服务器 PositionCorrection 回执，将角色吸附回权威位置。
 * 位移/朝向变化低于阈值时跳过上报，减少无效流量。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UPlayerMovementReportComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerMovementReportComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 上报频率（秒/次），默认 10Hz
    UPROPERTY(EditDefaultsOnly, Category = "MovementReport")
    float ReportIntervalSec = 0.1f;

    // 位移阈值（m）：距上次上报位移小于该值则跳过
    UPROPERTY(EditDefaultsOnly, Category = "MovementReport")
    float MinMoveDistance = 5.0f;

    // 朝向变化阈值（度）：距上次上报朝向变化小于该值则跳过
    UPROPERTY(EditDefaultsOnly, Category = "MovementReport")
    float MinYawDelta = 2.0f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandlePositionCorrection(FVector Location, float Yaw);

    void ReportMovement();
    void SnapToServerSpawn(UGameServerSubsystem* GameServer);
    UGameServerSubsystem* GetGameServer() const;

    float AccumulatedTime = 0.0f;
    FVector LastReportedLocation = FVector::ZeroVector;
    float LastReportedYaw = 0.0f;
    bool bHasReported = false;
};
