// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Types/CustomMovementModeTypes.h"
#include "Types/MovementStateTypes.h"
#include "OpenWorldARPGCharacterMovementComponent.generated.h"

class UClimbingComponent;

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

	// ==========================================
	// 缓存指针 (初始化阶段设置，物理循环中零开销访问)
	// ==========================================

	/** 缓存的 Owner Character (避免每帧 Cast) */
	UPROPERTY()
	TObjectPtr<ACharacter> CachedOwnerCharacter;

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
};
