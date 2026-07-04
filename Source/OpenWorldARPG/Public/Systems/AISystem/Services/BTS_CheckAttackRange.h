// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_CheckAttackRange.generated.h"

/**
 * 行为树服务节点：定时检测 AI Pawn 与目标 Actor 之间的距离，
 * 将"是否在攻击范围内"的判定结果写入黑板。
 *
 * 性能优化：使用 DistSquared 比较，避免开平方运算。
 * 解耦设计：仅依赖 APawn，不引入具体角色类。
 */
UCLASS()
class OPENWORLDARPG_API UBTS_CheckAttackRange : public UBTService
{
    GENERATED_BODY()

public:
    UBTS_CheckAttackRange();

    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    /** 攻击距离阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackRange = 200.0f;

    /** 目标 Actor 黑板键 */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    /** 输出结果黑板键（是否在攻击范围内） */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector IsInAttackRangeKey;
};
