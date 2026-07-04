// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/AISystem/AIControllers/OpenWorldARPGAIController.h" 
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyController.generated.h"

class UBehaviorTree;

UCLASS()
class OPENWORLDARPG_API AEnemyController : public AOpenWorldARPGAIController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

    UFUNCTION()
    virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

protected:
    // --- 配置：AI 核心 ---

    /** 行为树资产 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Config")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    // --- 配置：黑板键名 ---

    /** 敌人家位置键名 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName HomeLocationKeyName = FName("HomeLocation");

    /** 目标角色键名 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName TargetActorKeyName = FName("TargetActor");

    // --- 配置：状态过滤 ---
    
    /** 目标只要拥有此容器中任意一个 Tag，敌人就忽略该目标（死亡、待机等） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Tags")
    FGameplayTagContainer IgnoreTargetTags;
};