// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_UnmountVehicleBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class AWheeledVehiclePawnBase;

/**
 * 下车能力。由载具 InputExitVehicle → Server_UnPossessVehicle 发送 UnmountVehicleEventTag 激活。
 *
 * 生命周期（延迟物理脱离）：
 * 1. ActivateAbility：角色此时仍然 Attach 在车座上，在载具局部坐标系中播放下车蒙太奇，
 *    天然保持与车辆的相对静止（即使车辆在移动）。
 * 2. OnMontageCompleted / OnMontageInterrupted：
 *    - 获取角色当前世界坐标（动画已将角色带到车外）
 *    - 此时才调用 PlayerChar->EndDriving(FinalExitLocation) 执行真正的物理脱离
 *      （DetachFromActor + 恢复碰撞 + 恢复移动模式）
 * 3. EndAbility
 *
 * 架构原则：控制权移交（Controller）与物理脱离（GA）解耦，
 *           角色在车内完成下车动画后再解除与车辆的物理 Attach。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_UnmountVehicleBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_UnmountVehicleBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 下车蒙太奇（跨出车门） */
    UPROPERTY(EditDefaultsOnly, Category = "UnmountVehicle|Config")
    UAnimMontage* UnmountMontage;

    /** 执行物理脱离：获取角色当前世界坐标，调用 EndDriving */
    void ExecutePhysicalUnbind();

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

    /** 缓存目标载具（从角色的 Attachment Parent 获取） */
    AWheeledVehiclePawnBase* TargetVehicle = nullptr;

    /** 防止重复执行物理脱离 */
    bool bHasUnbound = false;
};
