// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/AttackTypes.h"
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

    void ExecuteAttack();
    void AttackOrientation();
    void ApplyDamageToTargets();
    void ClearAllTasks();
    void CorrectPawnOrient();

    // --- 回调 ---

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnDamageEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnComboOpenEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnComboCloseEventReceived(FGameplayEventData Payload);

    UFUNCTION()
    void OnNextAttackInputReceived(FGameplayEventData Payload);

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Type")
    EAttackType AttackType = EAttackType::Normal;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DamageDealEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowOpenTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ComboWindowCloseTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag NextAttackInputTag;

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
    int32 ComboIndex = 0;
    bool bCanTriggerAttack = true;

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
};