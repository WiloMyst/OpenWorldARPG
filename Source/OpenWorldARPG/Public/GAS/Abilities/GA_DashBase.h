// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_DashBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class APlayerCharacter;

/**
 * 冲刺/闪避技能基类。按下左 Shift 触发。
 * - 有移动输入时：向移动方向冲刺一小段距离
 * - 无移动输入时：向角色后方闪避一小段距离
 *
 * 位移通过 FRootMotionSource_MoveToForce 实现，兼容 CMC 网络预测与回滚。
 *
 * 状态机流转：
 * 1. 按下 Shift → 无脑激活 GA_Dash（扣除瞬间体力、赋予无敌帧 Tag、播放蒙太奇）
 * 2. GA_Dash 蒙太奇即将结束时检测：Shift 仍按住 + 有移动输入 + 体力足够
 *    → 成立：发送 SprintStart 事件，由 GA_Sprint 接管
 *    → 不成立：GA_Dash 正常结束，角色回到普通行走
 * 3. GA_Sprint 持续扣减体力，移动输入归零或体力耗尽时自动结束
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_DashBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_DashBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 计算冲刺方向并应用 RootMotionSource 位移 */
    void ApplyDashMovement();

    /** 位移完成回调 */
    UFUNCTION()
    void OnDashFinished();

    /** 蒙太奇 BlendOut 回调：检测是否应过渡到 Sprint */
    UFUNCTION()
    void OnMontageBlendOut();

    /** 蒙太奇结束/打断回调 */
    UFUNCTION()
    void OnMontageFinished();

    /** 检测是否满足进入 Sprint 的条件，满足则触发 Sprint */
    void TryTransitionToSprint();

    // --- 配置 ---

    /** 冲刺蒙太奇（有移动输入时播放） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> DashMontage;

    /** 后闪蒙太奇（无移动输入时播放） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> BackDashMontage;

    /** 冲刺位移距离 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float DashDistance = 600.0f;

    /** 冲刺位移持续时间 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float DashDuration = 0.3f;

    /** 判定"有移动输入"的阈值（输入向量长度平方） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float MovementInputThreshold = 0.1f;

    /** 进入 Sprint 所需的最低体力 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Stamina")
    float SprintTransitionStaminaThreshold = 10.0f;

    /** 触发 GA_Sprint 的事件 Tag */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStartEventTag;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    uint16 DashRMS_ID = 0;
    bool bHasActiveRMS = false;
    bool bTransitioningToSprint = false;
};
