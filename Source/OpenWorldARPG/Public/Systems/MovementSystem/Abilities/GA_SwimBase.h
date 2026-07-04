// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SwimBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 游泳技能基类。当 CMC 进入 Swimming 自定义移动模式时自动激活。
 *
 * 职责划分：
 * - GA 管"意愿和表现"：体力消耗、快速游泳切换、溺水处理
 * - CMC 管"怎么动"：物理参数、水面吸附、出水检测
 *
 * 生命周期：
 * 1. CMC 检测入水 → 切换到 Swimming 自定义模式 → 注入 SwimmingTag
 * 2. GA 被 SwimmingTag 触发激活 → 应用体力持续扣除 GE
 * 3. 监听 Shift 输入 → 切换快速游泳（加速+加大体力消耗）
 * 4. 体力归零 → 触发溺水逻辑（扣除生命、传送回安全点）
 * 5. 离开水体 → CMC 退出游泳模式 → GA 自动结束
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SwimBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SwimBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 收到退出游泳事件（离开水体时 CMC 发送） */
    UFUNCTION()
    void OnStopSwimEventReceived(FGameplayEventData Payload);

    /** 收到快速游泳开始事件（Shift 按下时由 Controller 发送） */
    UFUNCTION()
    void OnFastSwimStartEventReceived(FGameplayEventData Payload);

    /** 收到快速游泳结束事件（Shift 释放时由 Controller 发送） */
    UFUNCTION()
    void OnFastSwimStopEventReceived(FGameplayEventData Payload);

    /** 定时检测游泳条件（体力耗尽、是否仍在水中） */
    void CheckSwimConditions();

    /** 体力归零时的溺水处理 */
    void HandleDrowning();

    // --- 配置 ---

    /** 停止游泳的事件 Tag（离开水体时由 CMC 发送） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    FGameplayTag StopSwimEventTag;

    /** 快速游泳开始事件 Tag（Shift 按下时由 Controller 发送） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    FGameplayTag FastSwimStartEventTag;

    /** 快速游泳结束事件 Tag（Shift 释放时由 Controller 发送） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    FGameplayTag FastSwimStopEventTag;

    /** 溺水事件 Tag（体力归零时发送，用于触发溺水动画/特效） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    FGameplayTag DrowningEventTag;

    /** 普通游泳持续扣减体力的 GameplayEffect 类 */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    TSubclassOf<UGameplayEffect> SwimStaminaDrainGE;

    /** 快速游泳持续扣减体力的 GameplayEffect 类（消耗更大） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    TSubclassOf<UGameplayEffect> FastSwimStaminaDrainGE;

    /** 溺水时扣除最大生命值比例的 GameplayEffect 类 */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    TSubclassOf<UGameplayEffect> DrowningDamageGE;

    /** 体力耗尽阈值，低于此值触发溺水 */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    float DrowningStaminaThreshold = 0.0f;

    /** 状态检测间隔（秒） */
    UPROPERTY(EditDefaultsOnly, Category = "Swim|Config")
    float ConditionCheckInterval = 0.2f;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitStopEventTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitFastSwimStartTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitFastSwimStopTask;

    /** 当前激活的体力扣除 GE Handle */
    FActiveGameplayEffectHandle StaminaDrainGEHandle;

    /** 状态检测定时器 */
    FTimerHandle ConditionCheckTimer;

    /** 是否正在快速游泳 */
    bool bIsFastSwimming = false;

    /** 是否已经触发溺水（防止重入） */
    bool bHasDrowned = false;
};
