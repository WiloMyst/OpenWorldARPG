// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SwapOutBase.generated.h"

class APlayerCharacter;
class UNiagaraSystem;

/** 退场取消委托：GA_SwapOutBase 验证失败或被取消时广播，由 Controller 监听 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwapOutCancelled, APlayerCharacter*, SwappedOutCharacter);

/**
 * 角色退场技能。由 Controller 在服务端触发。
 *
 * 流程：激活 → 验证移动模式 → 保存Transform → 播放退场特效 → 进入StandbyMode → EndAbility
 *
 * 验证机制：ActivateAbility 中检查 AllowedSwapOutMovementModes，
 * 不满足则直接 Cancel，Controller 通过 OnSwapOutCancelled 委托感知。
 *
 * 退场完成通知：EndAbility 时通过 APlayerCharacter::OnSwapOutCompleted
 * 委托广播，Controller 监听执行 Possess。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_SwapOutBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SwapOutBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /** 验证当前移动模式是否允许退场 */
    bool ValidateSwapOutConditions() const;

    /** 保存当前 Transform 供 GA_SwapInBase 使用 */
    void SaveSwapTransform();

    /** 播放退场视觉表现（特效、隐藏网格体等） */
    void PlaySwapOutVisuals();

    /** 进入待机休眠状态 */
    void EnterStandbyMode();

    // --- 配置：视觉 ---

    /** 退场特效位置偏移（相对于角色位置） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

    /** 退场特效缩放 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

    /** 退场特效 Niagara 系统 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Visuals")
    TObjectPtr<UNiagaraSystem> SwapOutFX;

    // --- 配置：切换验证 ---

    /** 允许切换出的移动模式数组（为空时表示不限制） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Validation")
    TArray<TEnumAsByte<EMovementMode>> AllowedSwapOutMovementModes;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    /** 保存的切换 Transform，供 Controller 读取后传给 GA_SwapInBase */
    FTransform SavedSwapTransform;

public:
    /** 获取保存的切换 Transform */
    const FTransform& GetSavedSwapTransform() const { return SavedSwapTransform; }

    /** 获取允许切换出的移动模式（供外部查询） */
    const TArray<TEnumAsByte<EMovementMode>>& GetAllowedSwapOutMovementModes() const { return AllowedSwapOutMovementModes; }
};
