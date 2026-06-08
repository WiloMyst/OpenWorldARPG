// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Types/CustomMovementModeTypes.h"
#include "Types/MovementStateTypes.h"
#include "GameplayTagContainer.h"
#include "OpenWorldARPGCharacterMovementComponent.generated.h"

class UClimbingComponent;
class UAbilitySystemComponent;

/**
 * @class UOpenWorldARPGCharacterMovementComponent
 * @brief 自定义角色移动组件，为 ECustomMovementMode 中的每种自定义模式提供 PhysCustom 实现。
 *
 * 架构原则：
 * - CMC 负责"怎么动"（物理模拟、碰撞、网络复制）
 * - ActorComponent（如 ClimbingComponent、MovementStateMachineComponent）负责"什么时候动"（检测、状态切换）
 * - 三者通过 MovementMode 和 EMovementState 通信，CMC 不依赖任何 ActorComponent
 * - 输入走引擎原生管线 (AddMovementInput → ConsumeInputVector)，CMC 不持有输入缓存
 * - 状态由 CustomMovementMode 枚举唯一确定，不维护冗余布尔值
 *
 * 性能原则：
 * - PhysCustom 每帧可能执行多次（网络回滚/低帧率），内部禁止 FindComponentByClass
 * - 所有外部指针在初始化阶段缓存
 * - 禁止在物理循环中使用 Kismet 延迟节点（MoveComponentTo 等）
 */
UCLASS()
class OPENWORLDARPG_API UOpenWorldARPGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UOpenWorldARPGCharacterMovementComponent();

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	// 重写引擎原生的移动模式改变事件
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** 初始化缓存指针 (供 PlayerCharacter::InitializeCharacter 调用) */
	void CacheOwnerReferences();

	// ==========================================
	// 攀爬移动接口 (供 ClimbingComponent 调用)
	// ==========================================

	/** 设置当前贴墙法线 (ClimbingComponent 检测到墙壁时调用) */
	void SetClimbWallNormal(const FVector& InNormal);

	/** 清除攀爬运行时状态 (退出攀爬时调用) */
	void ClearClimbState();

	/** 设置攀爬吸附目标 (进入攀爬时由 ClimbingComponent 调用，替代 MoveComponentTo) */
	void SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime);

	// ==========================================
	// 翻越接口 (供 ClimbingComponent 调用)
	// ==========================================

	/** 设置翻越目标 (ClimbingComponent 调用后切换到 MOVE_Custom/ClimbUp) */
	void SetClimbUpTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation);

	// ==========================================
	// 滑翔移动接口 (供 GA_GlideBase 调用)
	// ==========================================

	/** 进入滑翔模式 (GA 激活时调用，CMC 自行读取物理参数配置并接管运动状态) */
	void EnterGlideMode();

	/** 退出滑翔模式 (GA 结束时调用，CMC 恢复原始物理参数) */
	void ExitGlideMode();

	/** 查询是否正在滑翔 (基于 CustomMovementMode) */
	bool IsGliding() const;

	// ==========================================
	// 冲刺移动接口 (供 GA_SprintBase 调用)
	// ==========================================

	/** 进入冲刺模式 (GA 激活时调用，CMC 修改 MaxWalkSpeed) */
	void EnterSprintMode();

	/** 退出冲刺模式 (GA 结束时调用，CMC 恢复原始 MaxWalkSpeed) */
	void ExitSprintMode();

	/** 查询是否正在冲刺 */
	bool IsSprinting() const;

	// ==========================================
	// 慢走移动接口 (供 GA_WalkBase 调用)
	// ==========================================

	/** 进入慢走模式 (GA 激活时调用，CMC 修改 MaxWalkSpeed) */
	void EnterWalkMode();

	/** 退出慢走模式 (GA 结束时调用，CMC 恢复原始 MaxWalkSpeed) */
	void ExitWalkMode();

	/** 查询是否正在慢走 */
	bool IsWalking() const;

	// ==========================================
	// 瞄准移动接口 (供 GA_AimBase 调用)
	// ==========================================

	/** 进入瞄准模式 (GA 激活时调用，CMC 修改 MaxWalkSpeed、旋转模式) */
	void EnterAimMode();

	/** 退出瞄准模式 (GA 结束时调用，CMC 恢复原始参数) */
	void ExitAimMode();

	/** 查询是否正在瞄准 */
	bool IsAiming() const;

	// ==========================================
	// 下落状态接口 (供 MovementStateMachineComponent 调用)
	// ==========================================

	/** 设置下落状态旋转插值速率 (进入下落时由 FSM 设置) */
	void SetFallingRotationInterpSpeed(float InSpeed);

	/** 获取下落状态旋转插值速率 */
	float GetFallingRotationInterpSpeed() const { return FallingRotationInterpSpeed; }

	// ==========================================
	// 墙角过渡接口 (供 ClimbingComponent 调用)
	// ==========================================

	/** 设置墙角过渡目标位置和法线 (同时切换 CustomMovementMode 为 ClimbingCornerTransition) */
	void SetCornerTransitionTarget(const FVector& InTargetLocation, const FVector& InTargetNormal, ECornerType InCornerType);

	/** 清除墙角过渡状态 (同时将 CustomMovementMode 恢复为 Climbing) */
	void ClearCornerTransition();

	/** 查询是否正在墙角过渡中 (基于 CustomMovementMode，无冗余布尔值) */
	bool IsInCornerTransition() const;

	// ==========================================
	// 状态标签配置 (替代原本 GA 里的配置)
	// ==========================================

	/** 处于空中时的基础标签 (Character.State.InAir.Airborne) */
	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag AirborneTag;

	/** 处于掉落时的标签 (Character.State.InAir.Falling) */
	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag FallingTag;

	/** 处于滑翔时的标签 (Character.State.InAir.Gliding) */
	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag GlidingTag;

	/** 落地时发送的兜底取消滑翔事件 (Input.Action.Glide.Stop) */
	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag StopGlideEventTag;

	// ==========================================
	// 攀爬配置 (从 FMovementStateConfigs 同步)
	// ==========================================

	/** 攀爬贴墙移动速度 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbMoveSpeed = 150.0f;

	/** 攀爬时转身面对墙壁的插值平滑速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbRotationInterpSpeed = 10.0f;

	/** 贴墙移动时期望离墙面的距离 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float TargetWallDistance = 30.0f;

	/** 攀爬时的重力衰减 (0=无重力, 1=满重力) */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbGravityScale = 0.0f;

	// ==========================================
	// 墙角过渡配置
	// ==========================================

	/** 阳角过渡：位置插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Convex")
	float ConvexPositionInterpSpeed = 5.0f;

	/** 阳角过渡：旋转插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Convex")
	float ConvexRotationInterpSpeed = 8.0f;

	/** 阴角过渡：位置插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Concave")
	float ConcavePositionInterpSpeed = 6.0f;

	/** 阴角过渡：旋转插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|Concave")
	float ConcaveRotationInterpSpeed = 10.0f;

	/** 墙角过渡完成阈值：位置距离 */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|General")
	float CornerArrivalThreshold = 2.0f;

	/** 墙角过渡完成阈值：角度差 (度) */
	UPROPERTY(EditDefaultsOnly, Category = "CornerTransition|General")
	float CornerRotationArrivalThreshold = 3.0f;

	// ==========================================
	// 地面配置 (CMC 统一管理物理参数)
	// ==========================================

	/** 地面行走时的旋转速率 */
	UPROPERTY(EditDefaultsOnly, Category = "Grounded|Physics")
	FRotator GroundedRotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// ==========================================
	// 下落配置 (CMC 统一管理物理参数)
	// ==========================================

	/** 下落时的空中控制力 (0~1) */
	UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
	float FallingAirControl = 0.1f;

	/** 下落时的旋转速率 */
	UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
	FRotator FallingRotationRate = FRotator(0.0f, 300.0f, 0.0f);

	// ==========================================
	// 滑翔配置 (CMC 统一管理所有物理参数，GA 不持有/不传递)
	// ==========================================

	/** 滑翔时的重力缩放 (0=无重力下坠, 1=正常重力) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideGravityScale = 0.0f;

	/** 滑翔时的空中控制力 (0~1) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideAirControl = 0.8f;

	/** 滑翔时的旋转速率 */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	FRotator GlideRotationRate = FRotator(0.0f, 0.0f, 100.0f);

	/** 进入滑翔时的初始弹射速度 (世界空间) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	FVector GlideLaunchVelocity = FVector(0.0f, 0.0f, -200.0f);

	/** 滑翔最大水平速度 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMaxHorizontalSpeed = 600.0f;

	/** 滑翔最小下落速度 (cm/s，负值表示向下) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMinDescentSpeed = -200.0f;

	/** 滑翔最大下落速度 (cm/s，负值表示向下) */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMaxDescentSpeed = -50.0f;

	// ==========================================
	// 冲刺配置 (CMC 统一管理物理参数，GA 不持有/不传递)
	// ==========================================

	/** 冲刺时的最大行走速度 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint|Physics")
	float SprintMaxWalkSpeed = 1200.0f;

	// ==========================================
	// 慢走配置 (CMC 统一管理物理参数，GA 不持有/不传递)
	// ==========================================

	/** 慢走时的最大行走速度 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Walk|Physics")
	float WalkMaxWalkSpeed = 200.0f;

	// ==========================================
	// 瞄准配置 (CMC 统一管理物理参数，GA 不持有/不传递)
	// ==========================================

	/** 瞄准时的最大行走速度 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	float AimMaxWalkSpeed = 450.0f;

	/** 瞄准时是否使用控制器旋转偏航 */
	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	bool AimUseControllerRotationYaw = true;

	/** 瞄准时是否朝向移动方向旋转 */
	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	bool AimOrientRotationToMovement = false;

	// ==========================================
	// 翻越配置
	// ==========================================

	/** 翻越位置插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpPositionInterpSpeed = 8.0f;

	/** 翻越旋转插值速率 */
	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpRotationInterpSpeed = 10.0f;

	/** 翻越完成阈值：位置距离 */
	UPROPERTY(EditDefaultsOnly, Category = "ClimbUp")
	float ClimbUpArrivalThreshold = 3.0f;

private:
	/** 攀爬物理模拟 */
	void PhysClimbing(float DeltaTime, int32 Iterations);

	/** 墙角过渡物理模拟 */
	void PhysCornerTransition(float DeltaTime, int32 Iterations);

	/** 翻越物理模拟 */
	void PhysClimbUp(float DeltaTime, int32 Iterations);

	/** 滑翔物理模拟 */
	void PhysGliding(float DeltaTime, int32 Iterations);

	// ==========================================
	// 缓存指针 (初始化阶段设置，物理循环中零开销访问)
	// ==========================================

	/** 缓存的 Owner Character (避免每帧 Cast) */
	UPROPERTY()
	TObjectPtr<ACharacter> CachedOwnerCharacter;

	// 缓存技能系统组件，实现零开销交互
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	/** 缓存的 ClimbingComponent (避免每帧 FindComponentByClass) */
	UPROPERTY()
	TObjectPtr<UClimbingComponent> CachedClimbingComp;

	// 攀爬运行时状态 (输入由引擎管线管理，不在此缓存)
	FVector ClimbWallNormal = FVector::ZeroVector;

	// 攀爬吸附状态 (替代 MoveComponentTo)
	bool bIsSnappingToWall = false;
	FVector SnapTargetLocation = FVector::ZeroVector;
	FRotator SnapTargetRotation = FRotator::ZeroRotator;
	float SnapInterpSpeed = 10.0f; // 由 WallSnapTime 反算

	// 下落旋转插值
	float FallingRotationInterpSpeed = 3.0f;

	// 墙角过渡运行时状态 (状态由 CustomMovementMode 标识，不维护布尔值)
	FVector CornerTargetLocation = FVector::ZeroVector;
	FVector CornerTargetNormal = FVector::ZeroVector;
	ECornerType CornerType = ECornerType::None;

	// 翻越运行时状态
	FVector ClimbUpTargetLocation = FVector::ZeroVector;
	FRotator ClimbUpTargetRotation = FRotator::ZeroRotator;

	// 滑翔运行时状态 (CMC 管理，GA 不再直接操作物理参数)
	float OriginalGravityScale = 1.0f;
	float OriginalAirControl = 0.05f;
	FRotator OriginalRotationRate = FRotator::ZeroRotator;

	// 冲刺/慢走运行时状态 (CMC 管理，GA 不再直接操作 MaxWalkSpeed)
	float OriginalMaxWalkSpeed = 600.0f;
	bool bIsSprinting = false;
	bool bIsWalking = false;

	// 瞄准运行时状态 (CMC 管理，GA 不再直接操作运动参数)
	bool bIsAiming = false;
	bool bOriginalOrientRotationToMovement = true;
	bool bOriginalUseControllerRotationYaw = false;
};
