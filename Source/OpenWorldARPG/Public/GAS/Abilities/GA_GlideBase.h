// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_GlideBase.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * @class UGA_GlideBase
 * @brief 滑翔技能基类。
 *
 * 架构原则：GA 管意愿和表现，CMC 管物理和运动状态。
 * - GA 负责：施加状态 GE、生成/销毁滑翔伞、监听停止事件、发送空中状态变更事件
 * - CMC 负责：修改/恢复物理参数(GravityScale/AirControl/RotationRate)、LaunchCharacter、PhysGliding 物理模拟、Tag 管理
 * - GA 不持有任何物理参数，不越权访问 CMC 的职责
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

    // ==========================================
    // 策划配置项 (意愿和表现层)
    // ==========================================

    /** 滑翔状态 GE (赋予 Character.State.InAir.Gliding 等 Tag) */
    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    TSubclassOf<UGameplayEffect> GlideStateEffectClass;

    /** 滑翔伞 Actor 类 (表现层：视觉外观) */
    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    TSubclassOf<AActor> GliderActorClass;

    /** 滑翔伞挂载的骨骼 Socket 名 */
    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    FName GliderSocketName = FName("GliderSocket");

    /** 停止滑翔的事件 Tag (Input.Action.Glide.Stop) */
    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    FGameplayTag StopGlideEventTag;

    /** 空中状态变更事件 Tag (通知 FSM/其他系统) */
    UPROPERTY(EditDefaultsOnly, Category = "Glide|Config")
    FGameplayTag AirStateChangedEventTag;

private:
    /** 运行时缓存：滑翔状态 GE 句柄 */
    FActiveGameplayEffectHandle ActiveGlideEffectHandle;

    /** 运行时缓存：生成的滑翔伞实例 */
    UPROPERTY()
    TObjectPtr<AActor> SpawnedGlider;

    /** 运行时缓存：等待停止事件的异步任务 */
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitEventTask;
};
