// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/AIControllers/EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Characters/AI/EnemyCharacter.h" 
#include "Characters/PlayerCharacter.h" 

void AEnemyController::BeginPlay()
{
    Super::BeginPlay();

    // 绑定感知更新事件 (等同于蓝图的红框事件节点)
    if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
    {
        PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyController::OnTargetPerceptionUpdated);
    }
}

void AEnemyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 1. 对应蓝图：运行行为树
    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
    }

    // 2. 对应蓝图：向黑板写入巡逻坐标 (HomeLocation)
    if (AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(InPawn))
    {
        if (UBlackboardComponent* BB = GetBlackboardComponent())
        {
            // 如果配了巡逻区域，就用巡逻区域的位置；否则兜底用自己的初始出生位置
            if (EnemyChar->PatrolArea)
            {
                BB->SetValueAsVector(HomeLocationKeyName, EnemyChar->PatrolArea->GetActorLocation());
            }
            else
            {
                BB->SetValueAsVector(HomeLocationKeyName, InPawn->GetActorLocation());
            }
        }
    }
}

void AEnemyController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB || !Actor) return;

    // 对应蓝图的分支：Successfully Sensed (是否成功感知)
    if (Stimulus.WasSuccessfullySensed())
    {
        // 对应蓝图：类型转换为 BP_PlayerCharacter
        if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Actor))
        {
            bool bShouldIgnore = false;

            // 检查目标是否拥有任意忽略 Tag（死亡、待机等）
            if (IgnoreTargetTags.Num() > 0)
            {
                UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerChar);
                if (TargetASC)
                {
                    FGameplayTagContainer OwnedTags;
                    TargetASC->GetOwnedGameplayTags(OwnedTags);
                    if (OwnedTags.HasAny(IgnoreTargetTags))
                    {
                        bShouldIgnore = true;
                    }
                }
            }

            // 如果玩家未被忽略，正式将其设为目标
            if (!bShouldIgnore)
            {
                BB->SetValueAsObject(TargetActorKeyName, PlayerChar);
            }
            else
            {
                // 目标无效（死亡/待机），清除黑板目标
                BB->ClearValue(TargetActorKeyName);
            }
        }
    }
    else
    {
        // 对应蓝图 False 分支：清除黑板数值
        BB->ClearValue(TargetActorKeyName);
    }
}