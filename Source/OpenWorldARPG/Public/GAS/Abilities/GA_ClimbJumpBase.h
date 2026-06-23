// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ClimbJumpBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;

/**
 * 攀爬跳跃衍生动作技能。
 *
 * 当角色处于攀爬状态并按下跳跃键时，PlayerCharacter 发送 ClimbJumpEventTag 事件激活此技能。
 * 根据输入意图分发为两种行为：
 * - W+空格（InputY >= 0.1）：向上冲刺，保持攀爬状态，播放冲刺蒙太奇，扣除体力
 * - S+空格/无输入（InputY < 0.1）：脱墙后空翻，退出攀爬，赋予反冲速度，播放后空翻蒙太奇
 *
 * 架构原则：
 * - 纯动画表现（向上冲刺）与物理状态切换（脱墙空翻）在同一技能内基于输入上下文分发
 * - 脱墙分支必须发送 ClimbStopEventTag 通知 GA_ClimbBase 结束，否则攀爬 GA 会残留
 * - 蒙太奇回调四件套（OnCompleted/OnBlendOut/OnInterrupted/OnCancelled）确保技能不卡死
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_ClimbJumpBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ClimbJumpBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 向上冲刺分支：保持攀爬，播放蒙太奇，扣除体力 */
    void ExecuteClimbDashUp(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);

    /** 脱墙后空翻分支：退出攀爬，赋予反冲速度，播放蒙太奇 */
    void ExecuteWallEject(const FGameplayAbilityActorInfo* ActorInfo);

    /** 蒙太奇正常播放完成 */
    UFUNCTION()
    void OnMontageCompleted();

    /** 蒙太奇 BlendOut（即将结束） */
    UFUNCTION()
    void OnMontageBlendOut();

    /** 蒙太奇被打断 */
    UFUNCTION()
    void OnMontageInterrupted();

    /** 蒙太奇被取消 */
    UFUNCTION()
    void OnMontageCancelled();

    // --- 配置 ---

    /** 向上冲刺蒙太奇（W+空格，建议带 Root Motion 向上位移） */
    UPROPERTY(EditDefaultsOnly, Category = "ClimbJump|Config")
    UAnimMontage* ClimbDashUpMontage;

    /** 脱墙后空翻蒙太奇（S+空格，建议原地动画，位移由 Velocity 驱动） */
    UPROPERTY(EditDefaultsOnly, Category = "ClimbJump|Config")
    UAnimMontage* WallEjectMontage;

    /** 向上冲刺扣除体力的 GameplayEffect 类 */
    UPROPERTY(EditDefaultsOnly, Category = "ClimbJump|Config")
    TSubclassOf<UGameplayEffect> StaminaCostGE;

    /** 停止攀爬事件 Tag（脱墙分支发送此事件通知 GA_ClimbBase 结束） */
    UPROPERTY(EditDefaultsOnly, Category = "ClimbJump|Config")
    FGameplayTag ClimbStopEventTag;

    /** 输入阈值：InputY >= 此值触发向上冲刺，否则触发脱墙后空翻 */
    UPROPERTY(EditDefaultsOnly, Category = "ClimbJump|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DashUpInputThreshold = 0.1f;

private:
    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

    /** 当前是否为脱墙分支（用于决定 EndAbility 时是否需要额外清理） */
    bool bIsWallEject = false;
};
