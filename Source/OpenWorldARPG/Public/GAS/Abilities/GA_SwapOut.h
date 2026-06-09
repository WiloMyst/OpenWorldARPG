// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SwapOut.generated.h"

class APlayerCharacter;
class UNiagaraSystem;

/**
 * 角色退场技能。由 Controller 在服务端触发。
 *
 * 流程：激活 → 保存Transform → 播放退场特效 → 进入StandbyMode → EndAbility
 *
 * 退场完成通知机制：GA_SwapOut 在 EndAbility 时通过 APlayerCharacter::OnSwapOutCompleted
 * 委托广播，Controller 监听该委托执行 Possess。这避免了 GA 直接引用 Controller 的循环依赖。
 *
 * 网络策略：LocalPredicted
 * - 服务端执行：完整退场逻辑 + 委托通知 Controller
 * - 客户端执行：视觉预测（特效、隐藏），不驱动 Possess
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SwapOut : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SwapOut();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 保存当前 Transform 供 GA_SwapIn 使用 */
    void SaveSwapTransform();

    /** 播放退场视觉表现（特效、隐藏网格体等） */
    void PlaySwapOutVisuals();

    /** 进入待机休眠状态 */
    void EnterStandbyMode();

    // --- 配置 ---

    /** 退场特效位置偏移（相对于角色位置） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

    /** 退场特效缩放 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

    /** 退场特效 Niagara 系统 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    TObjectPtr<UNiagaraSystem> SwapOutFX;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    /** 保存的切换 Transform，供 Controller 读取后传给 GA_SwapIn */
    FTransform SavedSwapTransform;

public:
    /** 获取保存的切换 Transform */
    const FTransform& GetSavedSwapTransform() const { return SavedSwapTransform; }
};
