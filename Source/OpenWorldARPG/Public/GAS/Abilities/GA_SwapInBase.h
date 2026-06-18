// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SwapInBase.generated.h"

class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;

/**
 * 角色出场技能。由 Controller 在 Possess 完成后触发。
 *
 * 流程：激活 → 验证PreventSwitchTags → 解除StandbyMode → 设置Transform → 播放出场Montage → 赋予无敌GE → EndAbility
 *
 * Transform 传递：Controller 在激活 GA 前直接调用 SetActorTransform 设置角色位置，
 * GA 在 ActivateAbility 中从 GetActorTransform() 读取，无需 Character 暂存状态或事件数据传递。
 *
 * 网络策略：LocalPredicted
 * - 服务端和客户端均执行视觉表现
 * - 无敌GE通过 GAS 的 Mixed 复制模式自动同步
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SwapInBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SwapInBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 验证角色是否允许出场（检查 PreventSwitchTags） */
    bool ValidateSwapInConditions() const;

    /** 解除待机模式并设置 Transform */
    void ExitStandbyAndSetTransform();

    /** 播放出场蒙太奇 */
    void PlaySwapInMontage();

    /** 赋予无敌 GE（包含 State.Invincible 标签） */
    void ApplyInvincibleEffect();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

    // --- 配置：动画 ---

    /** 出场蒙太奇 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> SwapInMontage;

    // --- 配置：效果 ---

    /** 无敌 GE 类（需包含 State.Invincible 标签，持续时间如 1.5s） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> InvincibleEffectClass;

    // --- 配置：切换验证 ---

    /** 防止切换上场的状态标签容器（拥有这些标签时禁止出场） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Validation")
    FGameplayTagContainer PreventSwitchTags;

    // --- 配置：Tags ---

    /** 出场事件标签 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SwapInEventTag;

public:
    /** 获取防止切换标签（供外部查询） */
    const FGameplayTagContainer& GetPreventSwitchTags() const { return PreventSwitchTags; }

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    /** 出场 Transform，从 TriggerEventData 读取 */
    FTransform SwapInTransform;
};
