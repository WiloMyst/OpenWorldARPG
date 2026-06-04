// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetSelectionComponent.generated.h"

class ACharacter;
class APlayerController;

/** 当最优目标发生变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBestTargetChanged, AActor*, OldTarget, AActor*, NewTarget);

/**
 * @class UTargetSelectionComponent
 * @brief 通用的视野目标选取组件。负责维护周围有效目标列表，并基于摄像机朝向筛选最优目标。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UTargetSelectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetSelectionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ==========================================
    // 供目标基类或外部调用的数组管理接口
    // ==========================================

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void AddTarget(AActor* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void RemoveTarget(AActor* TargetToRemove);

    /** 获取当前视野中最优目标 */
    UFUNCTION(BlueprintPure, Category = "Targeting")
    AActor* GetBestTarget() const { return CurrentBestTarget; }

    UPROPERTY(BlueprintAssignable, Category = "Targeting|Events")
    FOnBestTargetChanged OnBestTargetChanged;

protected:
    virtual void BeginPlay() override;

    // ==========================================
    // 内部核心算法
    // ==========================================

    float CalculateAngleDot(AActor* Target) const;
    AActor* FindBestTarget() const;

protected:
    // ==========================================
    // 配置项
    // ==========================================

    /** 判定为有效目标的最小点积阈值 (越大要求越精准正对) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Config")
    float MinDotThreshold = 0.3f;

private:
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    UPROPERTY()
    TArray<AActor*> AvailableTargets;

    UPROPERTY()
    TObjectPtr<AActor> CurrentBestTarget;
};