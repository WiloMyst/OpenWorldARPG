// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
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

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

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