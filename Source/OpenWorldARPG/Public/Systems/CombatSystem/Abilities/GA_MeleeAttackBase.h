// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Systems/CombatSystem/Data/CharacterCombatDataAsset.h"
#include "GA_MeleeAttackBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class APlayerCharacter;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_MeleeAttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_MeleeAttackBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    /**
     * 执行指定节点的攻击。
     * @param NodeName 要执行的连招节点名称。如果 NAME_None，使用连招图入口节点。
     */
    void ExecuteAttack(FName NodeName);

    /**
     * 索敌检测 + Motion Warping 目标设置。
     * 废弃 SetActorRotation，完全由 Motion Warping 处理位移和朝向吸附。
     * @param MaxWarpDistance 当前节点配置的最大吸附距离
     */
    void AttackOrientation(float MaxWarpDistance);

    /** 对检测范围内的敌人上报攻击命中（Phase 3: 服务器权威伤害） */
    void ApplyDamageToTargets();

    /** 清理所有 Ability Task */
    void ClearAllTasks();

    /** 修正角色朝向（归零 Pitch/Roll） */
    void CorrectPawnOrient();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnDamageEventReceived(FGameplayEventData Payload);

    /** 连招窗口打开事件（AnimNotifyState_SendGameplayEvent 的 BeginEventTag） */
    UFUNCTION()
    void OnComboOpenEventReceived(FGameplayEventData Payload);

    /** 连招窗口关闭事件（AnimNotifyState_SendGameplayEvent 的 EndEventTag） */
    UFUNCTION()
    void OnComboCloseEventReceived(FGameplayEventData Payload);

    /** 玩家攻击输入事件（如 Input.Attack.Normal） */
    UFUNCTION()
    void OnAttackInputEventReceived(FGameplayEventData Payload);

    /**
     * 长按派生重击检查回调。
     * 动画播放到 AN_CheckHeavyAttackBranch 帧时触发，
     * 若玩家仍在按住普攻键，则模拟发送一次重击输入事件，
     * 顺滑走入连招图的重击分支。
     */
    UFUNCTION()
    void OnHeavyBranchCheckReceived(FGameplayEventData Payload);

protected:
    // --- 配置 ---

    /**
     * 天赋 Tag（数据驱动核心）。
     * 在 GA 蓝图实例中配置（如 Ability.Attack.Normal / Ability.Attack.Heavy），
     * 运行时通过此 Tag 从 CombatDataAsset->CharacterTalents 字典中查询对应的连招图。
     *
     * 优势：新增攻击类型（如 Ability.Attack.Plunge）只需新建 GA 蓝图并配置 Tag，
     * 无需修改 Character 的 C++ 代码或 EAttackType 枚举。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag TalentTag;

    /**
     * 服务器技能 ID（对齐 GameServer skills.yaml 的 skill_id）。
     * 攻击命中敌人时上报 DamageIntent，伤害由服务器裁决并以 DamageDeal 广播。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Server")
    int32 ServerSkillId = 1;

    /**
     * 默认伤害 GameplayEffect。
     * 当 FComboActionNode 未配置 DamageEffect 时回退使用此 GE。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    /** 伤害判定事件 Tag（由 AnimNotify_SendGameplayEvent 在攻击命中帧发送） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

    /**
     * 连招窗口打开事件 Tag。
     * 由 AnimNotifyState_SendGameplayEvent 的 BeginEventTag 发送。
     * 窗口打开时，立即检查并消费输入缓存，实现最快响应。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowOpenTag;

    /**
     * 连招窗口关闭事件 Tag。
     * 由 AnimNotifyState_SendGameplayEvent 的 EndEventTag 发送。
     * 引擎保证即使动画被打断，此事件也必定触发，确保窗口状态不会锁死。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowCloseTag;

    /**
     * 基础攻击输入 Tag（父级标签，如 Input.Attack）。
     * 在引擎蓝图里配置为父级标签，WaitGameplayEvent 的 OnlyMatchExact 设为 false，
     * 这样 Input.Attack.Normal 和 Input.Attack.Heavy 都能被监听到，
     * 实现轻重击随意穿插派生。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag BaseAttackInputTag;

    /**
     * 长按派生重击检查事件 Tag（如 Character.Event.CheckHeavyBranch）。
     * 由蒙太奇中的 AN_CheckHeavyAttackBranch AnimNotify 在指定帧发送。
     * GA 收到后检查玩家是否仍在按住普攻键，若是则模拟发送重击输入。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag HeavyBranchCheckTag;

    /**
     * 重击输入事件 Tag（如 Input.Attack.Heavy）。
     * 长按派生检查通过后，GA 用此 Tag 模拟发送重击输入事件，触发连招图的重击派生分支。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag HeavyAttackInputTag;

    /** Motion Warping Target 名称（需与蒙太奇中的 Motion Warping AnimNotifyState 匹配） */
    UPROPERTY(EditDefaultsOnly, Category = "Config|MotionWarping")
    FName WarpTargetName = FName("AttackTarget");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    float OrientRadius = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    TArray<TEnumAsByte<EObjectTypeQuery>> OrientObjectTypes;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Orientation")
    FName EnemyActorTag = FName("Enemy");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceRadius = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceForwardOffset1 = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceForwardOffset2 = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

private:
    /** 当前连招节点名称（替代旧的 ComboIndex） */
    FName CurrentNodeName;

    /** 当前节点缓存指针（避免每帧查表，运行时临时使用，不序列化） */
    const FComboActionNode* CurrentNode = nullptr;

    /** 当前正在播放的攻击蒙太奇（用于防串线校验） */
    UPROPERTY()
    TObjectPtr<UAnimMontage> ActiveAttackMontage;

    /** 连招窗口是否打开 */
    bool bComboWindowOpen = false;

    UPROPERTY()
    TArray<AActor*> HitActors;

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboOpenTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboCloseTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> InputTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> HeavyBranchCheckTask;
};
