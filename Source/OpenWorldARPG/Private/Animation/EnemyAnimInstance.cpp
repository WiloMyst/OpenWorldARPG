// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/EnemyAnimInstance.h"
#include "Characters/AI/EnemyCharacter.h"
#include "Core/AIControllers/EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"

UEnemyAnimInstance::UEnemyAnimInstance()
    : Super()
{
}

void UEnemyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

        // 额外缓存怪物专属指针
    
    if (AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(TryGetPawnOwner()))
    {
        CachedEnemyCharacter = EnemyChar;

        // 缓存 AIController（用于读取 Blackboard 中的仇恨数据）
        if (AAIController* AIController = Cast<AAIController>(EnemyChar->GetController()))
        {
            CachedAIController = AIController;
        }
    }
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // ================================================================
    // GameThread 快照：从 Blackboard 拉取仇恨数据
    // ================================================================
    // Blackboard 不是线程安全的，必须在主线程读取。
    // 基类 NativeUpdateAnimation 已快照通用物理数据（ActorLocation 等），
    // 此处直接复用基类快照计算距离。

    SnapshotDistanceToTarget = 0.0f;
    bSnapshotIsAggroed = false;
    bSnapshotHasTarget = false;
    SnapshotTargetLocation = FVector::ZeroVector;

    const AAIController* AIController = CachedAIController.Get();
    if (!AIController)
    {
        return;
    }

    const UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
    if (!Blackboard)
    {
        return;
    }

    // 读取目标 Actor
    if (AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKeyName)))
    {
        bSnapshotHasTarget = true;
        bSnapshotIsAggroed = true;
        SnapshotTargetLocation = TargetActor->GetActorLocation();

        // 计算与目标的距离（使用基类快照 SnapshotActorLocation）
        SnapshotDistanceToTarget = FVector::Dist(SnapshotActorLocation, SnapshotTargetLocation);
    }
}

void UEnemyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // 基类先更新通用数据
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    // bIsMoving：Enemy/NPC 只需速度超过阈值（AI 驱动，无玩家输入向量）
    bIsMoving = GroundSpeed > MoveSpeedThreshold;

    // ================================================================
    // Worker Thread 纯数据计算
    // ================================================================
    // 严禁出现任何 Character->Get...() 调用。
    // 使用基类快照 SnapshotActorLocation / SnapshotActorRotation。

    // --- 1. 仇恨数据（主线程快照）---

    DistanceToTarget = SnapshotDistanceToTarget;
    bIsAggroed = bSnapshotIsAggroed;

    // --- 2. 原地转身方向 (Turn-In-Place) ---
    //
    // 计算逻辑：当怪物静止时，计算当前朝向与目标方向的 Yaw 差值
    // 正值=需要右转，负值=需要左转
    // 驱动 Turn-In-Place 动画选择和播放速率

    if (bSnapshotHasTarget && !bIsMoving)
    {
        // 计算期望朝向：从角色位置指向目标位置（使用基类快照）
        const FVector DirectionToTarget = (SnapshotTargetLocation - SnapshotActorLocation).GetSafeNormal2D();

        // 将目标方向转换到本地空间，取 Yaw 差值（使用基类快照）
        const FVector LocalDirection = SnapshotActorRotation.UnrotateVector(DirectionToTarget);
        const float DesiredYaw = FRotationMatrix::MakeFromX(LocalDirection).Rotator().Yaw;

        // 超过阈值才更新，避免微小抖动
        TurnInPlaceDirection = FMath::Abs(DesiredYaw) > TurnThresholdDegrees ? DesiredYaw : 0.0f;
    }
    else
    {
        TurnInPlaceDirection = 0.0f;
    }

    // --- 3. Update Velocity (插值计算 Speed X 和 Speed Y) ---
    // 基于敌人实际移动速度计算，不依赖玩家输入

    DirectionCurrent = FMath::VInterpTo(DirectionCurrent, SnapshotVelocity, DeltaSeconds, 5.0f);

    // 使用基类快照的方向向量投影到本地坐标系
    SpeedX = FVector::DotProduct(DirectionCurrent, SnapshotActorRightVector);
    SpeedY = FVector::DotProduct(DirectionCurrent, SnapshotActorForwardVector);
}
