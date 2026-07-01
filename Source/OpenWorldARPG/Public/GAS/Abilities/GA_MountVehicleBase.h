// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MountVehicleBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class AWheeledVehiclePawnBase;

/**
 * 上车能力。由载具 OnInteract 发送 MountVehicleEventTag 激活。
 *
 * 生命周期：
 * 1. 从 EventData.OptionalObject 解析目标载具
 * 2. 获取交互吸附 Transform（车门位置），通过 Motion Warping 将角色平滑移动到车门
 * 3. 播放"拉车门坐下"蒙太奇
 * 4. 蒙太奇结束时，调用 Controller->Server_PossessVehicle 执行 Possess 切换
 * 5. EndAbility
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
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

    /** 缓存的目标载具（Transient，GA 生命周期短不需要弱引用追踪） */
    AWheeledVehiclePawnBase* TargetVehicle = nullptr;
};
