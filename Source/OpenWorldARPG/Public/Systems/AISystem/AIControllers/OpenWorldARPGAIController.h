// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "OpenWorldARPGAIController.generated.h"

class UAIPerceptionComponent;

/**
 * AI 控制器基类。搭建通用 AI 基础设施。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGAIController : public AAIController
{
    GENERATED_BODY()

public:
    AOpenWorldARPGAIController();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;
};