// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTT_FindPatrolLocation.generated.h"

/**
 * 行为树任务节点：在巡逻区域内随机寻找一个可达点。
 *
 * 工作流程：
 * 1. 从受控 Pawn（AEnemyCharacter）身上获取 PatrolArea
 * 2. 以 PatrolArea 的位置为原点，PatrolRadius 为半径
 * 3. 调用导航系统查询随机可达点
 * 4. 将结果写入黑板（键名由 PatrolLocationKey 配置）
 */
UCLASS()
class OPENWORLDARPG_API UBTT_FindPatrolLocation : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTT_FindPatrolLocation();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    /** 巡逻目标位置的黑板键 */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector PatrolLocationKey;
};
