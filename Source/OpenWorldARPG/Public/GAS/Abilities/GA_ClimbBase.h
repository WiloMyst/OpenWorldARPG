// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ClimbBase.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_PlayMontageAndWait;
class UMotionWarpingComponent;

/**
 * 攀爬技能基类。当 CMC 检测到可攀爬墙壁并发送 Event.Movement.TryClimb 时激活。
 *
 * 架构原则："GAS 掌控状态与生命周期 (大脑)，CMC 纯粹提供物理检测与运动模拟 (肌肉)"
 *
 * 职责划分：
 * - GA 管"意愿和表现"：技能激活/结束、体力消耗、Tag 管理、蒙太奇播放、Motion Warping 对齐
 * - CMC 管"怎么动"：物理参数、墙面吸附、射线检测、移动模拟
 *
 * 生命周期（含根运动过渡）：
 * 1. CMC 检测到可攀爬墙壁 → 发送 Event.Movement.TryClimb（携带 HitResult）
 * 2. GA 被 Event 触发激活 → 从 TriggerEventData 解析 HitResult
 * 3. GA 命令 CMC 进入攀爬物理模式 → 添加 State.Climbing.Transition Tag（锁定输入）
 * 4. GA 设置 Motion Warping Target → 播放上墙蒙太奇（Root Motion + Motion Warping 吸附到墙面）
 * 5. 蒙太奇播放结束 → 移除 Transition Tag → 应用体力消耗 GE → 监听退出事件
 * 6. 任何原因导致 GA 结束 → EndAbility → CMC->ExitClimb()
 *
 * Transition Tag 机制：
 * - State.Climbing.Transition：蒙太奇播放期间存在，用于拦截攀爬输入
 *   蒙太奇正常结束或被打断时移除，确保只有在过渡完成后才能自由攀爬
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_ClimbBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ClimbBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 上墙蒙太奇播放完成回调 */
    UFUNCTION()
    void OnTransitionMontageCompleted();

    /** 上墙蒙太奇被打断/取消回调 */
    UFUNCTION()
    void OnTransitionMontageInterrupted();

    /** 收到停止攀爬事件（CMC 检测到落地/离开墙壁，或玩家按跳跃退出时发送） */
    UFUNCTION()
    void OnStopClimbEventReceived(FGameplayEventData Payload);

    /**
     * 过渡完成后的初始化：移除 Transition Tag、应用体力 GE、监听退出事件。
     * 无论蒙太奇正常完成还是被打断，都调用此函数确保状态一致。
     */
    void OnTransitionFinished();

    // --- 配置 ---

    /** 上墙过渡蒙太奇（需配置 Motion Warping Anim Notify 匹配 ClimbStartWarpTargetName） */
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    UAnimMontage* TransitionMontage;

    /** Motion Warping Target 名称（需与蒙太奇中的 Anim Notify 匹配） */
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    FName ClimbStartWarpTargetName = FName("ClimbStartPoint");

    /** 攀爬过渡状态 Tag（蒙太奇期间额外存在，拦截攀爬输入） */
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    FGameplayTag ClimbTransitionTag;

    /** 停止攀爬事件 Tag（CMC 检测到落地时发送，或玩家按跳跃退出时发送，统一走此事件） */
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    FGameplayTag StopClimbEventTag;

    /** 攀爬持续扣减体力的 GameplayEffect 类 */
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Config")
    TSubclassOf<UGameplayEffect> ClimbStaminaDrainGE;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitStopEventTask;

    /** 当前激活的体力扣除 GE Handle */
    FActiveGameplayEffectHandle StaminaDrainGEHandle;

    /** 是否已完成过渡（蒙太奇播放完毕） */
    bool bTransitionFinished = false;
};
