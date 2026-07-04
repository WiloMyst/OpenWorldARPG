// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_NavMoveTo.generated.h"

class UPathFollowingComponent;
struct FAIRequestID;
struct FPathFollowingResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNavMoveToDelegate);

/**
 * 自定义 NavMesh 寻路 AbilityTask。
 *
 * 利用 UAIBlueprintHelperLibrary::SimpleMoveToLocation 发起基于 NavMesh 的真实寻路（避障），
 * 通过 UPathFollowingComponent::OnRequestFinished 委托监听寻路结果。
 *
 * 与原生的 UAbilityTask_MoveToLocation（穿模线性插值）不同，
 * 本 Task 会沿着 NavMesh 路径行走，自动绕开障碍物。
 *
 * 委托生命周期：
 * - Activate() 中绑定 OnRequestFinished
 * - OnPathFollowingFinished() 中立即解绑并广播结果
 * - OnDestroy() 中安全解绑并 AbortMove（防止 Ability 被外部取消时寻路残留）
 */
UCLASS()
class OPENWORLDARPG_API UAbilityTask_NavMoveTo : public UAbilityTask
{
    GENERATED_BODY()

public:
    /** 寻路成功到达目标 */
    UPROPERTY(BlueprintAssignable)
    FNavMoveToDelegate OnTargetReached;

    /** 寻路失败（无路径 / 被阻挡 / 被中断） */
    UPROPERTY(BlueprintAssignable)
    FNavMoveToDelegate OnFailed;

    /**
     * 创建 NavMesh 寻路 Task。
     * @param OwningAbility 拥有此 Task 的 GA
     * @param TargetLocation 目标位置（世界坐标）
     * @param AcceptanceRadius 可接受半径（cm），角色进入此范围即视为到达
     */
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
    static UAbilityTask_NavMoveTo* CreateNavMoveToTask(UGameplayAbility* OwningAbility, FVector TargetLocation, float AcceptanceRadius = 50.0f);

protected:
    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    /** PathFollowingComponent 寻路完成回调 */
    void OnPathFollowingFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

    /** 目标位置（世界坐标） */
    FVector TargetLocation = FVector::ZeroVector;

    /** 可接受半径 */
    float AcceptanceRadius = 50.0f;

    /** 缓存的 PathFollowingComponent（SimpleMoveToLocation 自动创建） */
    UPROPERTY()
    TObjectPtr<UPathFollowingComponent> PathFollowingComp = nullptr;

    /** OnRequestFinished 委托句柄，用于安全解绑 */
    FDelegateHandle PathFinishedHandle;
};
