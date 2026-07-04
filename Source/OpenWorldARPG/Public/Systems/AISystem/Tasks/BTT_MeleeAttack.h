// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_MeleeAttack.generated.h"

class AEnemyCharacter;
class UBehaviorTreeComponent;

/**
 * 行为树任务节点：命令敌人执行近战攻击，并挂起等待攻击完成委托回调。
 *
 * 工作流程：
 * 1. ExecuteTask：绑定 OnAttackFinished 委托，调用 MeleeAttack()，返回 InProgress
 * 2. 攻击完成：委托回调触发，解绑委托，调用 FinishLatentTask(Succeeded)
 * 3. AbortTask：被打断时解绑委托，返回 Aborted
 *
 * 注意：设置 bCreateNodeInstance = true，每个 BT 节点实例独立，
 * 确保 CachedOwnerComp/CachedEnemy 不会跨实例串扰。
 */
UCLASS()
class OPENWORLDARPG_API UBTT_MeleeAttack : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTT_MeleeAttack();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    /** 攻击完成委托回调 */
    UFUNCTION()
    void OnAttackFinishedCallback();

protected:
    /** 缓存异步期间的 OwnerComp，回调时用于结束挂起任务 */
    UPROPERTY()
    TObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

    /** 缓存异步期间的敌人指针，回调时用于解绑委托 */
    UPROPERTY()
    TObjectPtr<AEnemyCharacter> CachedEnemy;
};
