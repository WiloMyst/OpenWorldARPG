// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/OpenWorldARPGAnimInstance.h"
#include "EnemyAnimInstance.generated.h"

class AEnemyCharacter;
class AAIController;

/**
 * 怪物专属动画实例。追加仇恨/转身等 AI 驱动状态。
 */
UCLASS()
class OPENWORLDARPG_API UEnemyAnimInstance : public UOpenWorldARPGAnimInstance
{
    GENERATED_BODY()

public:
    UEnemyAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    // --- Aggro ---

    /** 与仇恨目标的距离 (cm)，驱动战斗/巡逻状态机切换 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Aggro")
    float DistanceToTarget = 0.0f;

    /** 是否处于激怒/战斗状态 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Aggro")
    bool bIsAggroed = false;

    // --- Turn In Place ---

    /** 原地转身角度偏差 (度)，正值=右转，负值=左转 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|TurnInPlace")
    float TurnInPlaceDirection = 0.0f;

private:
    TWeakObjectPtr<AEnemyCharacter> CachedEnemyCharacter;
    TWeakObjectPtr<AAIController> CachedAIController;

    // --- 主线程快照 (Blackboard 非线程安全) ---

    float SnapshotDistanceToTarget = 0.0f;
    bool bSnapshotIsAggroed = false;
    FVector SnapshotTargetLocation = FVector::ZeroVector;
    bool bSnapshotHasTarget = false;

    FName TargetActorKeyName = FName("TargetActor");
    static constexpr float TurnThresholdDegrees = 5.0f;
};
