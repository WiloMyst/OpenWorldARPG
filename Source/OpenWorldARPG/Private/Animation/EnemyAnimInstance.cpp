// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/EnemyAnimInstance.h"
#include "Characters/AI/EnemyCharacter.h"
#include "Core/AIControllers/EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"

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

        // GameThread 快照：从 Blackboard 拉取仇恨数据
    // Blackboard 不是线程安全的，必须在主线程读取
    
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

        // 计算与目标的距离
        const ACharacter* Character = CachedCharacter.Get();
        if (Character)
        {
            SnapshotDistanceToTarget = FVector::Dist(Character->GetActorLocation(), SnapshotTargetLocation);
        }
    }
}

void UEnemyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // 基类先更新通用数据
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    const ACharacter* Character = CachedCharacter.Get();
    if (!Character)
    {
        return;
    }

        // 1. 仇恨数据（主线程快照）
    
    DistanceToTarget = SnapshotDistanceToTarget;
    bIsAggroed = bSnapshotIsAggroed;

        // 2. 原地转身方向 (Turn-In-Place)
    //
    // 计算逻辑：当怪物静止时，计算当前朝向与目标方向的 Yaw 差值
    // 正值=需要右转，负值=需要左转
    // 驱动 Turn-In-Place 动画选择和播放速率
    
    if (bSnapshotHasTarget && !bIsMoving)
    {
        // 计算期望朝向：从角色位置指向目标位置
        const FVector DirectionToTarget = (SnapshotTargetLocation - Character->GetActorLocation()).GetSafeNormal2D();
        const FRotator CurrentRotation = Character->GetActorRotation();

        // 将目标方向转换到本地空间，取 Yaw 差值
        const FVector LocalDirection = CurrentRotation.UnrotateVector(DirectionToTarget);
        const float DesiredYaw = UKismetMathLibrary::MakeRotFromX(LocalDirection).Yaw;

        // 超过阈值才更新，避免微小抖动
        TurnInPlaceDirection = FMath::Abs(DesiredYaw) > TurnThresholdDegrees ? DesiredYaw : 0.0f;
    }
    else
    {
        TurnInPlaceDirection = 0.0f;
    }
}
