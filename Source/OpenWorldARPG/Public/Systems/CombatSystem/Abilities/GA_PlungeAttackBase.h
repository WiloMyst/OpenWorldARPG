// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_PlungeAttackBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitMovementModeChange;
class APlayerCharacter;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_PlungeAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_PlungeAttackBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    void ExecuteAttack();
    void ApplyDamageToTargets();
    void ClearAllTasks();
    void CorrectPawnOrient();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

    // 落地蒙太奇播放完成的回调
    UFUNCTION()
    void OnLandingMontageFinished();

    UFUNCTION()
    void OnMovementModeChanged(EMovementMode NewMovementMode);

    UFUNCTION()
    void OnDamageEventReceived(FGameplayEventData Payload);

protected:
    // --- 配置 ---

    /**
     * 天赋 Tag（数据驱动核心）。
     * 在 GA 蓝图实例中配置（如 Ability.Attack.Plunge），
     * 运行时通过此 Tag 从 CombatDataAsset->CharacterTalents 字典中查询对应的连招图。
     * 替代旧架构中硬编码的 FGameplayTag::RequestGameplayTag("Ability.Attack.Plunge")。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag TalentTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

    /**
     * 落地流转触发 Tag（事件驱动状态机核心）。
     *
     * 【设计理念：将 GameplayTag 作为状态机事件触发器】
     * ComboGraph 的 NextNodes 是一个 TMap<FGameplayTag, FName> 派生表，
     * Key 是输入/事件 Tag，Value 是目标节点名称。
     * 当角色落地时，使用此 Tag（如 Event.Movement.Landed）去 NextNodes 中
     * 精准 Find() 对应的落地节点名称，而非遍历整个表取第一个。
     *
     * 优势：
     * - 精准匹配：一个节点可以配置多条派生路径（落地、被打断、超时等），
     *   各自用不同的 Tag 区分，互不干扰
     * - 可扩展：策划可以自由添加新的事件 Tag（如 Event.Plunge.Cancel），
     *   无需修改 C++ 代码
     * - 语义清晰：Tag 本身即文档，读配置表即可理解流转意图
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag LandedTransitionTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float PlungeDamageRadius = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

private:
    bool bInterruptedByLand = false;

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> FallMontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> LandingMontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitMovementModeChange> MovementModeTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageTask;
};