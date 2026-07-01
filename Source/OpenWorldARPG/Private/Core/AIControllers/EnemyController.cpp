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

    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
    }

    // 向黑板写入巡逻坐标
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

    if (Stimulus.WasSuccessfullySensed())
    {
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
        BB->ClearValue(TargetActorKeyName);
    }
}