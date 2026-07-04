// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIPatrolAreaBase.generated.h"

/**
 * AI 巡逻区域基类。
 * 定义一个圆形巡逻范围，AI 在此半径内进行巡逻移动。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AAIPatrolAreaBase : public AActor
{
    GENERATED_BODY()

public:
    AAIPatrolAreaBase();

    /** 巡逻半径 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol", meta = (ClampMin = "0.0"))
    float PatrolRadius = 700.0f;
};
