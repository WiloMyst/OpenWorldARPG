// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "OpenWorldARPGAIController.generated.h"

class UAIPerceptionComponent;

/**
 * @class AOpenWorldARPGAIController
 * @brief 游戏中所有 AI 控制器的纯 C++ 基类，负责搭建通用的 AI 基础设施。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGAIController : public AAIController
{
    GENERATED_BODY()

public:
    AOpenWorldARPGAIController();

protected:
    // ==========================================
    // 核心 AI 基础设施
    // ==========================================

    /**
     * 通用的 AI 感知组件。
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;
};