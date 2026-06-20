// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/OpenWorldARPGAnimInstance.h"
#include "PlayerAnimInstance.generated.h"

class APlayerCharacter;
class APlayerController;

/**
 * 玩家专属动画实例。追加瞄准/锁定/冲刺等玩家专属状态。
 *
 * 【多线程安全】
 * - 所有 CMC 状态读取（IsSprinting/IsWalking/IsAiming/IsClimbing 等）
 *   已移至 NativeUpdateAnimation（主线程），存入 Snapshot... 变量。
 * - NativeThreadSafeUpdateAnimation 只读取快照进行纯数学运算。
 * - 基类已上提的通用快照（SnapshotActorRotation/Location/ForwardVector 等）直接复用，
 *   不再在子类重复定义。
 */
UCLASS()
class OPENWORLDARPG_API UPlayerAnimInstance : public UOpenWorldARPGAnimInstance
{
    GENERATED_BODY()

public:
    UPlayerAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    // --- Locomotion & Pivot ---

    /** 归一化后的输入加速度向量 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    FVector AccelerationVector = FVector::ZeroVector;

    /** 用于行走状态方向插值的当前向量 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    FVector DirectionCurrent = FVector::ZeroVector;

    /** 混合空间 X 轴分量 (左右) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    double SpeedX = 0.0f;

    /** 混合空间 Y 轴分量 (前后) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    double SpeedY = 0.0f;

    /** 是否处于急停/碎步停止状态 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    bool bInStepStopping = false;

    /** 是否在左足停止 */
    UPROPERTY(BlueprintReadOnly, Category = "Movement|Stop")
    bool bStopOnLeftFoot = false;

    /** 触发急停瞬间的速度大小，用于驱动不同的停止动画过渡 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Locomotion")
    double SpeedOnStop = 0.0f;

    /** 是否允许在急停状态下更新速度 (由动画状态机或通知决定) */
    UPROPERTY(BlueprintReadWrite, Category = "AnimData|Locomotion")
    bool bCanSetSpeedOnStep = false;

    // --- Aim ---

    /** 瞄准俯仰角 (度)，驱动上半身/头部 AimOffset 混合空间 [-90, 90] */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Aim")
    double AimPitch = 0.0f;

    /** 瞄准偏航角 (度)，驱动上半身扭转 [-180, 180] */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Aim")
    double AimYaw = 0.0f;

    /** 脊椎旋转（用于复杂的上半身扭转动画） */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Aim")
    FRotator SpineRotation = FRotator::ZeroRotator;

    // --- Player States（工作线程计算输出，蓝图只读）---

    /** 是否在冲刺 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|PlayerState")
    bool bIsSprinting = false;

    /** 是否在慢走 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|PlayerState")
    bool bIsWalking = false;

    /** 是否在瞄准 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|PlayerState")
    bool bIsAiming = false;

    /** 是否在攀爬 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsClimbing = false;

    /** 是否在滑翔 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsGliding = false;

    /** 是否在游泳（自定义游泳模式） */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsSwimming = false;

    /** 是否在快速游泳 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|MovementState")
    bool bIsFastSwimming = false;

    // --- Transition ---

    /** 是否从急停过渡到地面移动 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Transitions")
    bool bShouldStopStep2GroundMove = false;

    /** 是否从空中过渡到地面移动 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Transitions")
    bool bShouldAirborne2GroundMove = false;

    /** 是否从地面移动过渡到跳跃开始 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Transitions")
    bool bShouldGroundMove2JumpStart = false;
    
    /** 是否从地面移动过渡到下落循环 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Transitions")
    bool bShouldGroundMove2FallLoop = false;
    
    /** 是否从跳跃开始过渡到下落循环 */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Transitions")
    bool bShouldJumpStart2FallLoop = false;

    // --- Input ---

    /** 当前 X 轴输入值 (前后) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Input")
    float InputX = 0.0f;

    /** 当前 Y 轴输入值 (左右) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Input")
    float InputY = 0.0f;

private:
    // --- 缓存指针 ---

    TWeakObjectPtr<APlayerCharacter> CachedPlayerCharacter;
    TWeakObjectPtr<APlayerController> CachedPlayerController;

    // ================================================================
    // 主线程快照（GameThread 写入，Worker Thread 只读）
    // ================================================================
    // 以下快照在 NativeUpdateAnimation 中从 PlayerController / CMC / SkeletalMesh 读取。
    // 通用物理快照（ActorRotation/Location/ForwardVector/RightVector/LastInputVector 等）
    // 已上提至基类，此处不再重复定义。

    // --- 瞄准数据快照 ---

    /** 主线程快照：控制器旋转（用于 AimOffset 计算） */
    FRotator SnapshotControlRotation = FRotator::ZeroRotator;

    // --- CMC 状态快照（非线程安全，必须主线程读取）---

    bool bSnapshotIsSprinting = false;
    bool bSnapshotIsWalking = false;
    bool bSnapshotIsAiming = false;
    bool bSnapshotIsClimbing = false;
    bool bSnapshotIsGliding = false;
    bool bSnapshotIsSwimming = false;
    bool bSnapshotIsFastSwimming = false;

    /** 主线程快照：CMC 最大速度（用于行走状态速度投影） */
    float SnapshotMaxSpeed = 0.0f;

    // --- 脚部骨骼位置快照 ---

    FVector SnapshotLeftFootLoc = FVector::ZeroVector;
    FVector SnapshotRightFootLoc = FVector::ZeroVector;

    // --- 蓝图可配置骨骼名 ---

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Stop")
    FName LeftFootBoneName = TEXT("Left-toe");

    UPROPERTY(EditDefaultsOnly, Category = "Movement|Stop")
    FName RightFootBoneName = TEXT("Right-toe");
};
