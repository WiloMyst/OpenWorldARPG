// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_GrappleHookBase.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class ACharacter;
class UNiagaraSystem;
class UNiagaraComponent;

UCLASS(Abstract)
class OPENWORLDARPG_API UGA_GrappleHookBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_GrappleHookBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // --- 核心流程 ---

    void OrientToTarget();
    void TriggerGrappleVFX();

    // --- 回调 ---

    UFUNCTION()
    void OnDelayFinished();

    UFUNCTION()
    void OnGrappleMoveFinished();

    UFUNCTION()
    void OnMontageFinished();

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    TObjectPtr<UAnimMontage> GrappleMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float HookDelay = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float GrappleMoveSpeed = 2500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    TObjectPtr<UNiagaraSystem> RopeVFXTemplate;

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName AttachSocketName = FName("Right-wrist");

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName RopeEndParamName = FName("End");

    UPROPERTY(EditDefaultsOnly, Category = "Config|VFX")
    FName RopeLifetimeParamName = FName("Lifetime");

private:
    UPROPERTY()
    TObjectPtr<ACharacter> CachedCharacter;

    UPROPERTY()
    TObjectPtr<AActor> CurrentHookTarget;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> SpawnedRopeVFX;

    uint16 GrappleRMS_ID = 0;
    bool bHasActiveRMS = false;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitDelay> DelayTask;
};