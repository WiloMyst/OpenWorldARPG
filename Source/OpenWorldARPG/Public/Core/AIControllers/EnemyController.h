// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGAIController.h" 
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Config")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    // --- 配置：黑板键名 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName HomeLocationKeyName = FName("HomeLocation");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName TargetActorKeyName = FName("TargetActor");

    // --- 配置：状态过滤 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Tags")
    FGameplayTag IgnoreStandbyTag;
};