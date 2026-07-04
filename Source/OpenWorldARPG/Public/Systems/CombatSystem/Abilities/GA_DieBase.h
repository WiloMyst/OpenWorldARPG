// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_DieBase.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * 死亡能力基类（表现层流水线）。
 * 流程：CancelAllAbilities -> HandleDeath(底层清理) -> 播放死亡蒙太奇 -> 开启布娃娃 -> EndAbility
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_DieBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_DieBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    /** 死亡蒙太奇。为空则跳过动画直接进入布娃娃/结束 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Animation")
    TObjectPtr<UAnimMontage> DeathMontage;

    /** 蒙太奇结束后是否开启布娃娃物理模拟 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Ragdoll")
    bool bEnableRagdollOnDeath = true;

    /** 蒙太奇播放结束回调（统一处理 BlendOut/Completed/Interrupted/Cancelled） */
    UFUNCTION()
    void OnDeathMontageFinished();

    /** 开启布娃娃物理模拟 */
    void StartRagdoll();

private:
    /** 播放蒙太奇任务实例缓存，用于生命周期管理 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
