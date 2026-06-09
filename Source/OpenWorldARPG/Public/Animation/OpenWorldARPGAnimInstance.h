// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "OpenWorldARPGAnimInstance.generated.h"

class UOpenWorldARPGCharacterMovementComponent;
class UAbilitySystemComponent;

/**
 * 动画实例基类。处理所有角色通用的物理/运动数据。
 * 多线程安全：NativeUpdateAnimation 快照 ASC Tags，NativeThreadSafeUpdateAnimation 在 Worker Thread 只读消费。
 */
UCLASS()
class OPENWORLDARPG_API UOpenWorldARPGAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UOpenWorldARPGAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    // --- Locomotion ---

    /** XY 轴速度大小 (cm/s)，驱动 Idle↔Move 混合空间 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float GroundSpeed = 0.0f;

    /** Z 轴速度 (cm/s)，正值=上升，负值=下落 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float VelocityZ = 0.0f;

    /** 是否有有效移动（速度超过阈值且有输入加速度） */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    bool bIsMoving = false;

    /** 本地速度方向 (0-1)，驱动 Idle↔Move 混合空间 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float LocalVelocityDirection = 0.0f;

    // --- Movement States ---

    /** 是否在地面 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsGrounded = false;

    /** 是否在下落 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsFalling = false;

    // --- GAS Tag 快照 ---

    /** 是否处于死亡状态 (Character.State.Dead) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|GASState")
    bool bIsDead = false;

    /** 是否处于不可控制状态 (Character.State.Uncontrollable) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|GASState")
    bool bIsUncontrollable = false;

protected:
    // --- 缓存指针 (TWeakObjectPtr，Worker Thread 安全) ---

    TWeakObjectPtr<ACharacter> CachedCharacter;
    TWeakObjectPtr<UOpenWorldARPGCharacterMovementComponent> CachedMovementComp;
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    // --- GAS Tag 快照 (主线程写入，工作线程只读) ---

    FGameplayTagContainer SnapshotGameplayTags;

    // --- 预构造 Tag (避免每帧 RequestGameplayTag 哈希查找) ---

    FGameplayTag DeadTag;
    FGameplayTag UncontrollableTag;

    static constexpr float MoveSpeedThreshold = 3.0f;
};
