// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "OpenWorldARPGAnimInstance.generated.h"

class UPlayerCharacterMovementComponent;
class UAbilitySystemComponent;

/**
 * 动画实例基类。处理所有角色通用的物理/运动数据。
 *
 * 【多线程安全架构】
 * - NativeUpdateAnimation（GameThread）：读取 Actor/Controller/MovementComponent 的所有非线程安全数据，
 *   存入 Snapshot... 成员变量。这是唯一允许访问 UObject 层 API 的地方。
 * - NativeThreadSafeUpdateAnimation（Worker Thread）：只读取 Snapshot... 变量进行纯数学运算，
 *   禁止出现任何 Character->Get...() 或 MoveComp->...() 调用，确保 Fast Path 真正生效。
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

    // --- Locomotion（工作线程计算输出，蓝图只读）---

    /** XY 轴速度大小 (cm/s)，驱动 Idle↔Move 混合空间 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float GroundSpeed = 0.0f;

    /** Z 轴速度 (cm/s)，正值=上升，负值=下落 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float VelocityZ = 0.0f;

    /** 是否有有效移动 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    bool bIsMoving = false;

    /** 本地速度方向角度，驱动方向混合空间 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    float LocalVelocityDirection = 0.0f;

    // --- Movement States（工作线程计算输出，蓝图只读）---

    /** 是否在地面 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsGrounded = false;

    /** 是否在下落 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsFalling = false;

    // --- GAS Tag 快照（工作线程计算输出，蓝图只读）---

    /** 是否处于死亡状态 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|GASState")
    bool bIsDead = false;

protected:
    // --- 缓存指针 (TWeakObjectPtr，Worker Thread 安全失效) ---

    TWeakObjectPtr<ACharacter> CachedCharacter;
    TWeakObjectPtr<UPlayerCharacterMovementComponent> CachedMovementComp;
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    // ================================================================
    // 主线程快照（GameThread 写入，Worker Thread 只读）
    // ================================================================
    // 以下所有 Snapshot... 变量均在 NativeUpdateAnimation 中从
    // Actor / MovementComponent 读取，工作线程只做纯数学运算。
    // 子类（Player/Enemy/NPC）直接复用这些通用快照，避免重复定义。

    // --- 通用物理快照（子类共享）---

    /** 主线程快照：角色速度（cm/s） */
    FVector SnapshotVelocity = FVector::ZeroVector;

    /** 主线程快照：剔除 MovementBase (移动平台) 速度后的真实相对速度 */
    FVector SnapshotRelativeVelocity = FVector::ZeroVector;

    /** 主线程快照：角色旋转 */
    FRotator SnapshotActorRotation = FRotator::ZeroRotator;

    /** 主线程快照：角色世界坐标 */
    FVector SnapshotActorLocation = FVector::ZeroVector;

    /** 主线程快照：角色前向向量 */
    FVector SnapshotActorForwardVector = FVector::ForwardVector;

    /** 主线程快照：角色右向向量 */
    FVector SnapshotActorRightVector = FVector::RightVector;

    /** 主线程快照：最后输入向量（来自 CMC） */
    FVector SnapshotLastInputVector = FVector::ZeroVector;

    /** 主线程快照：是否在地面（来自 CMC） */
    bool bSnapshotIsGrounded = false;

    /** 主线程快照：是否在下落（来自 CMC） */
    bool bSnapshotIsFalling = false;

    // --- GAS Tag 快照 ---

    FGameplayTagContainer SnapshotGameplayTags;

    // --- 蓝图可配置 Tag ---

    /** 死亡状态 Tag */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AnimData|GASState")
    FGameplayTag DeadTag;

    // --- 常量 ---

    static constexpr float MoveSpeedThreshold = 3.0f;
};
