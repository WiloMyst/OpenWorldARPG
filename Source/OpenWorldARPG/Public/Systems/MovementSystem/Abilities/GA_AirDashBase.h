// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AirDashBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitMovementModeChange;
class APlayerCharacter;

/**
 * 空中冲刺/闪避技能基类。在滞空状态下按下左 Shift 触发。
 * - 有移动输入时：向移动方向空中冲刺一小段距离（水平位移，短暂滞停下落）
 * - 无移动输入时：向角色后方空中闪避一小段距离
 *
 * 位移通过 FRootMotionSource_MoveToForce 实现，兼容 CMC 网络预测与回滚。
 *
 * 【权责分工】
 * - GA_AirDashBase：管"意愿和表现"（激活条件、蒙太奇、RMS 生命周期、落地监听）
 * - CMC：管"怎么动"（物理/碰撞/网络复制/重力）
 * - GAS：管"资源"（体力 Cost GE、冷却 Cooldown GE、状态 Tag）
 * - 蓝图配置：管"数据"（蒙太奇引用、位移参数、ActivationRequiredTags=FallingTag）
 *
 * 与 GA_DashBase 的关键差异：
 * 1. 仅在 IsFalling() 状态下可激活（通过蓝图 ActivationRequiredTags 配置 FallingTag）
 * 2. 位移仅作用于水平面（XY），Z 轴保持不变，形成短暂滞停感
 * 3. 位移结束后恢复下落（OnAirDashFinished 重置 Z 速度为 0，重力自然接管）
 * 4. 不触发 Sprint 过渡（空中无法疾跑）
 * 5. 监听落地事件，落地时立即结束技能并清理 RMS
 *
 * 鸣潮体力规则：AirDash 一次性扣除体力（由 GAS 原生 Cost GE 机制处理）。
 *
 * 蓝图配置指南：
 * - ActivationRequiredTags: 配置 FallingTag，确保仅在滞空时激活
 * - Cost GameplayEffect: 配置体力消耗 GE
 * - Cooldown GameplayEffect: 可选，配置冷却 GE（如需限制空中冲刺次数）
 * - ActivationOwnedTags: 配置无敌帧 Tag 等
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_AirDashBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AirDashBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 计算空中冲刺方向并应用 RootMotionSource 位移 */
    void ApplyAirDashMovement();

    /** 位移完成回调：设置惯性速度并结束技能 */
    UFUNCTION()
    void OnAirDashFinished();

    /** 蒙太奇结束/打断回调 */
    UFUNCTION()
    void OnMontageFinished();

    /** 落地回调：清理位移并结束技能 */
    UFUNCTION()
    void OnLanded(EMovementMode NewMovementMode);

    // --- 配置 ---

    /** 空中冲刺蒙太奇（有移动输入时播放） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> AirDashMontage;

    /** 空中后闪蒙太奇（无移动输入时播放） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> AirBackDashMontage;

    /** 空中冲刺位移距离（水平） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float AirDashDistance = 400.0f;

    /** 空中冲刺位移持续时间 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float AirDashDuration = 0.25f;

    /** AirDash 结束时保留的水平惯性速度（cm/s），前冲用，后撤步清零 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement", meta = (ClampMin = "0.0"))
    float AirDashEndInertiaSpeed = 600.0f;

    /** 判定"有移动输入"的阈值（输入向量长度平方） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float MovementInputThreshold = 0.1f;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitMovementModeChange> LandingTask;

    uint16 AirDashRMS_ID = 0;
    bool bHasActiveRMS = false;

    /** 在触发瞬间记录本次位移是否为后撤步 */
    bool bIsBackDash = false;

    /** 缓存本次冲刺方向，供 OnAirDashFinished 计算惯性速度 */
    FVector CachedDashDirection = FVector::ZeroVector;
};
