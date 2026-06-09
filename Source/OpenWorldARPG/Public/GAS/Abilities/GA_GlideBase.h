// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_GlideBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 滑翔技能基类。GA 管意愿和表现（滑翔伞生成/销毁、监听停止事件），
 * CMC 管物理参数和被动运动状态 Tag。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UGA_GlideBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_GlideBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UFUNCTION()
    void OnStopGlideEventReceived(FGameplayEventData Payload);

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    TSubclassOf<AActor> GliderActorClass;

    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    FName GliderSocketName = FName("GliderSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    FGameplayTag StopGlideEventTag;

private:
    UPROPERTY()
    TObjectPtr<AActor> SpawnedGlider;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
