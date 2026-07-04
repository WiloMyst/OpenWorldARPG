// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AISystem/Tasks/BTT_FindPatrolLocation.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/AICharacter/EnemyCharacter.h"
#include "Systems/AISystem/AIPatrolAreaBase.h"
#include "AIController.h"
#include "NavigationSystem.h"

UBTT_FindPatrolLocation::UBTT_FindPatrolLocation()
{
    NodeName = "Find Patrol Location";
}

EBTNodeResult::Type UBTT_FindPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 1. 获取受控 Pawn
    APawn* ControlledPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
    if (!ControlledPawn)
    {
        return EBTNodeResult::Failed;
    }

    // 2. 类型转换为 AEnemyCharacter
    AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(ControlledPawn);
    if (!EnemyChar)
    {
        return EBTNodeResult::Failed;
    }

    // 3. 获取巡逻数据
    AAIPatrolAreaBase* PatrolArea = EnemyChar->PatrolArea;
    if (!IsValid(PatrolArea))
    {
        return EBTNodeResult::Failed;
    }

    // 以巡逻区域位置为原点，巡逻半径为搜索半径
    const FVector Origin = PatrolArea->GetActorLocation();
    const float Radius = PatrolArea->PatrolRadius;

    // 4. 导航查询：在半径内随机寻找可达点
    FNavLocation NavLocation;
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSystem)
    {
        return EBTNodeResult::Failed;
    }

    const bool bFound = NavSystem->GetRandomReachablePointInRadius(Origin, Radius, NavLocation);

    // 5. 写入黑板并返回
    if (!bFound)
    {
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        return EBTNodeResult::Failed;
    }

    BlackboardComp->SetValueAsVector(PatrolLocationKey.SelectedKeyName, NavLocation.Location);
    return EBTNodeResult::Succeeded;
}
