// Copyright 2025 WiloMyst. All Rights Reserved.

#include "AI/Tasks/BTT_MeleeAttack.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/AI/EnemyCharacter.h"
#include "AIController.h"

UBTT_MeleeAttack::UBTT_MeleeAttack()
{
    NodeName = "Melee Attack";
    // 每个节点实例独立，确保缓存指针不会跨实例串扰
    bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTT_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 1. 获取受控 Pawn 并转换为 AEnemyCharacter
    APawn* ControlledPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
    AEnemyCharacter* EnemyPawn = Cast<AEnemyCharacter>(ControlledPawn);
    if (!EnemyPawn)
    {
        return EBTNodeResult::Failed;
    }

    // 2. 缓存异步上下文
    CachedOwnerComp = &OwnerComp;
    CachedEnemy = EnemyPawn;

    // 3. 绑定委托（先 Remove 防重，防止重复绑定导致回调触发多次）
    CachedEnemy->OnAttackFinished.RemoveDynamic(this, &UBTT_MeleeAttack::OnAttackFinishedCallback);
    CachedEnemy->OnAttackFinished.AddDynamic(this, &UBTT_MeleeAttack::OnAttackFinishedCallback);

    // 4. 发起攻击
    CachedEnemy->MeleeAttack();

    // 5. 挂起，等待 OnAttackFinishedCallback 回调
    return EBTNodeResult::InProgress;
}

void UBTT_MeleeAttack::OnAttackFinishedCallback()
{
    // 容错检查：确保缓存指针有效
    if (!CachedOwnerComp || !CachedEnemy)
    {
        return;
    }

    // 解绑委托，防止内存泄漏和下一次执行时触发两次
    CachedEnemy->OnAttackFinished.RemoveDynamic(this, &UBTT_MeleeAttack::OnAttackFinishedCallback);

    // 结束挂起任务
    FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTT_MeleeAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 被高优先级条件打断时清理现场
    if (CachedEnemy)
    {
        // 解绑委托，防止野指针回调
        CachedEnemy->OnAttackFinished.RemoveDynamic(this, &UBTT_MeleeAttack::OnAttackFinishedCallback);
    }

    // 清理缓存指针
    CachedOwnerComp = nullptr;
    CachedEnemy = nullptr;

    return EBTNodeResult::Aborted;
}
