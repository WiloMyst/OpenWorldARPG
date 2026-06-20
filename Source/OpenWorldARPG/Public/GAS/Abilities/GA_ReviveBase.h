// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ReviveBase.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;

/**
 * 复活能力基类（表现层流水线）。
 * 流程：StopRagdoll -> HandleRevive(底层还原) -> 应用回血/无敌GE -> 播放起身蒙太奇 -> EndAbility
 * 与 GA_DieBase 对称，彻底逆转死亡时的影响。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_ReviveBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ReviveBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    /** 起身蒙太奇。为空则跳过动画直接结束 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive|Animation")
    TObjectPtr<UAnimMontage> ReviveMontage;

    /** 回血 GE：复活时恢复血量 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive|Effects")
    TSubclassOf<UGameplayEffect> HealEffectClass;

    /** 无敌帧 GE：复活后短暂无敌 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive|Effects")
    TSubclassOf<UGameplayEffect> InvincibleEffectClass;

    /** 蒙太奇播放结束回调（统一处理 BlendOut/Completed/Interrupted/Cancelled） */
    UFUNCTION()
    void OnReviveMontageFinished();

    /** 关闭布娃娃物理，恢复动画蓝图控制 */
    void StopRagdoll();

private:
    /** 播放蒙太奇任务实例缓存，用于生命周期管理 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
