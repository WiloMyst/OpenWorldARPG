// Copyright 2025 WiloMyst. All Rights Reserved.

#include "AI/Services/BTS_CheckAttackRange.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTS_CheckAttackRange::UBTS_CheckAttackRange()
{
    NodeName = TEXT("Check Attack Range");

    // 开启 Tick
    bNotifyTick = true;

    // 限制黑板键类型：TargetActorKey 只能选 Object
    TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTS_CheckAttackRange, TargetActorKey), AActor::StaticClass());

    // 限制黑板键类型：IsInAttackRangeKey 只能选 Boolean
    IsInAttackRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTS_CheckAttackRange, IsInAttackRangeKey));
}

void UBTS_CheckAttackRange::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    APawn* ControlledPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;

    // 获取目标 Actor
    AActor* TargetActor = Cast<AActor>(BlackboardComp ? BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName) : nullptr);

    // 有效性校验：Pawn、黑板或目标无效时，直接写入 false
    if (!ControlledPawn || !BlackboardComp || !TargetActor)
    {
        if (BlackboardComp)
        {
            BlackboardComp->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, false);
        }
        return;
    }

    // 性能优化：使用距离平方比较，避免开平方运算
    const float DistSquared = FVector::DistSquared(ControlledPawn->GetActorLocation(), TargetActor->GetActorLocation());
    const float AttackRangeSquared = AttackRange * AttackRange;

    // 写入结果
    BlackboardComp->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, DistSquared <= AttackRangeSquared);
}
