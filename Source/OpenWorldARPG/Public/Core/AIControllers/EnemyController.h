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

    // 对应蓝图：目标感知更新时 (AIPerceptionComponent)
    UFUNCTION()
    virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

protected:
    // ==========================================
    // 配置项：AI 核心
    // ==========================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Config")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    // ==========================================
    // 配置项：黑板键名 (避免硬编码)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName HomeLocationKeyName = FName("HomeLocation");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
    FName TargetActorKeyName = FName("TargetActor");

    // ==========================================
    // 配置项：状态过滤
    // ==========================================

    /** 玩家拥有此 Tag 时，AI 视为看不见 (对应蓝图 Character.State.InStandby) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Tags")
    FGameplayTag IgnoreStandbyTag;
};