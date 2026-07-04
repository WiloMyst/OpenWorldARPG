// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MountVehicleBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_NavMoveTo;
class UAnimMontage;
class AWheeledVehiclePawnBase;

/**
 * 上车能力。由载具 OnInteract 发送 MountVehicleEventTag 激活。
 *
 * 生命周期（3A 级三阶段流程，基于两个 AbilityTask 串联）：
 *
 * 1. Approach（NavMesh 寻路）：
 *    - 解析目标载具，获取车门 InteractionTargetTransform
 *    - 使用自定义 UAbilityTask_NavMoveTo 沿 NavMesh 寻路避障走向车门
 *    - OnTargetReached → 进入阶段 2
 *    - OnFailed → EndAbility 取消上车
 *
 * 2. Bind & Align（物理绑定 + 转身对齐 + Motion Warping）：
 *    - 角色转身朝向车辆 (FindLookAtRotation)
 *    - Attach 到载具 Mesh（KeepWorldTransform），提前进入载具局部坐标系
 *    - 关闭胶囊体碰撞与移动，防止挤飞载具
 *    - 使用 AddOrUpdateWarpTargetFromComponent + bFollowComponent=true
 *      使 Motion Warping 动态跟随车门，播放"拉车门坐下"蒙太奇
 *
 * 3. Transfer（交接控制权）：
 *    - 蒙太奇结束后调用 Server_PossessVehicle
 *
 * 回调流 (Callback Flow)：
 *   ActivateAbility → NavMoveTo.OnTargetReached → OnApproachReached
 *      → AttachToComponent + WarpTarget + PlayMontage
 *      → Montage.OnCompleted → ExecuteMount → Server_PossessVehicle → EndAbility
 *
 * 架构原则：动画表现由 GA 控制，物理状态切换由 Controller Server RPC 执行
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_MountVehicleBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_MountVehicleBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** Motion Warping 吸附目标名称（需与蒙太奇中的 Anim Notify 匹配） */
    UPROPERTY(EditDefaultsOnly, Category = "MountVehicle|Config")
    FName WarpTargetName = FName("VehicleEntry");

    /** 上车蒙太奇（拉车门坐下） */
    UPROPERTY(EditDefaultsOnly, Category = "MountVehicle|Config")
    UAnimMontage* MountMontage;

    /** 寻路可接受半径（cm），角色进入此范围即视为到达车门 */
    UPROPERTY(EditDefaultsOnly, Category = "MountVehicle|Config", meta = (ClampMin = "10.0"))
    float ApproachAcceptanceRadius = 80.0f;

    // --- 阶段 1：寻路回调 ---

    /** NavMoveTo 到达车门后触发阶段 2 */
    UFUNCTION()
    void OnApproachReached();

    /** NavMoveTo 寻路失败，取消上车 */
    UFUNCTION()
    void OnApproachFailed();

    // --- 阶段 3：交接控制权 ---

    /** 是否已执行过上车操作（防止 BlendOut 和 Completed 重复触发） */
    bool bHasExecutedMount = false;

    /** 执行上车操作（合并 BlendOut 和 Completed 回调，加防重入保护） */
    void ExecuteMount();

    // --- 蒙太奇回调 ---

    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageBlendOut();

    UFUNCTION()
    void OnMontageInterrupted();

    UFUNCTION()
    void OnMontageCancelled();

private:
    /** 阶段 1 NavMesh 寻路 Task */
    UPROPERTY()
    TObjectPtr<UAbilityTask_NavMoveTo> NavMoveToTask;

    /** 阶段 2 蒙太奇 Task */
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

    /** 缓存的目标载具（Transient，GA 生命周期短不需要弱引用追踪） */
    AWheeledVehiclePawnBase* TargetVehicle = nullptr;
};
