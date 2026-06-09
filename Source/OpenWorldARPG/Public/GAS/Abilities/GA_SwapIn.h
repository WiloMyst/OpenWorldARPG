// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SwapIn.generated.h"

class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;

/**
 * 角色出场技能。由 Controller 在 Possess 完成后触发。
 *
 * 流程：激活 → 解除StandbyMode → 设置Transform → 播放出场Montage → 赋予无敌GE → EndAbility
 *
 * 网络策略：LocalPredicted
 * - 服务端和客户端均执行视觉表现
 * - 无敌GE通过 GAS 的 Mixed 复制模式自动同步
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SwapIn : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SwapIn();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 解除待机模式并设置 Transform */
    void ExitStandbyAndSetTransform();

    /** 播放出场蒙太奇 */
    void PlaySwapInMontage();

    /** 赋予无敌 GE（包含 State.Invincible 标签） */
    void ApplyInvincibleEffect();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

    // --- 配置 ---

    /** 出场蒙太奇 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> SwapInMontage;

    /** 无敌 GE 类（需包含 State.Invincible 标签，持续时间如 1.5s） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> InvincibleEffectClass;

    /** 出场时接收的 Transform（通过 TriggerEventData 传入，或由 Controller 在激活前设置到角色上） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SwapInEventTag;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    /** 缓存出场 Transform，由 Controller 在激活前写入角色 */
    FTransform SwapInTransform;
};
