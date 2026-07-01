// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class ACharacter;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBestTargetChanged, AActor*, OldTarget, AActor*, NewTarget);

/** 视野目标选取组件，基于摄像机朝向筛选最优目标。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetingComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- 目标管理 ---

    void AddTarget(AActor* NewTarget);
    void RemoveTarget(AActor* TargetToRemove);
    AActor* GetBestTarget() const { return CurrentBestTarget; }

protected:
    virtual void BeginPlay() override;

    // --- 内部算法 ---

    float CalculateAngleDot(AActor* Target) const;
    AActor* FindBestTarget() const;

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Targeting|Events")
    FOnBestTargetChanged OnBestTargetChanged;

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Config")
    float MinDotThreshold = 0.3f;

private:
    // --- 运行时状态 ---

    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    UPROPERTY()
    TArray<AActor*> AvailableTargets;

    UPROPERTY()
    TObjectPtr<AActor> CurrentBestTarget;
};
