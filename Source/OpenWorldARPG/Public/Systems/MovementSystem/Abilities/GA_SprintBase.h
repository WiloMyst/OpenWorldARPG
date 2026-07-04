// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SprintBase.generated.h"

class UAbilityTask_WaitDelay;

/**
 * 极速跑技能基类。由 GA_Dash 结束时检测条件后自动触发。
 *
 * 鸣潮规则：
 * - Dash 一次性消耗体力，Sprint 全程不消耗体力
 * - 点按 Dash + 有方向 → 冲刺后接续短时疾跑（ShortSprintDuration 秒后自动结束）
 * - 长按 Dash + 有方向 → 冲刺后进入持久疾跑（仅靠方向键维持，松开冲刺键不打断）
 * - 疾跑结束条件：移动输入归零 或（仅短疾跑）WaitDelay 到期
 *
 * GA 管意愿和表现，CMC 管物理参数。
 * 短疾跑使用 AbilityTask_WaitDelay 而非原生 Timer，确保网络预测键同步。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SprintBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SprintBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 短疾跑 WaitDelay 到期回调 */
    UFUNCTION()
    void OnShortSprintExpired();

    /** 定时检测移动输入状态 */
    void CheckSprintConditions();

    // --- 配置 ---

    /** 移动输入归零阈值（输入向量长度平方） */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float MovementInputThreshold = 0.1f;

    /** 状态检测间隔（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float ConditionCheckInterval = 0.1f;

    /** 短疾跑持续时间（秒），到期后自动结束 */
    UPROPERTY(EditDefaultsOnly, Category = "Sprint|Config")
    float ShortSprintDuration = 1.2f;

private:
    /** 状态检测定时器 */
    FTimerHandle ConditionCheckTimer;

    /** 短疾跑结束 AbilityTask（仅点按触发时启用，自动绑定预测键） */
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitDelay> ShortSprintDelayTask;

    /** 是否为短疾跑（点按触发） */
    bool bIsShortSprint = false;
};
