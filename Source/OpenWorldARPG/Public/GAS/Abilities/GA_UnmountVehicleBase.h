// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_UnmountVehicleBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class AWheeledVehiclePawnBase;

/**
 * 下车能力。由载具 InputExitVehicle 发送 UnmountVehicleEventTag 激活。
 *
 * 生命周期：
 * 1. 从 EventData.OptionalObject 解析目标载具
 * 2. 调用载具 FindSafeExitLocation 获取安全下车位置
 * 3. 调用 Controller->Server_UnPossessVehicle 执行 Possess 切换
 * 4. Controller 视角切回人后，播放跨出车门蒙太奇
 * 5. EndAbility
 *
 * 注意：下车蒙太奇在角色身上播放（Possess 切换后角色已被控制器接管）
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
};
