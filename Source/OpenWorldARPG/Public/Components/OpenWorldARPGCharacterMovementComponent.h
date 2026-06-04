// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "OpenWorldARPGCharacterMovementComponent.generated.h"

/**
 * @class UOpenWorldARPGCharacterMovementComponent
 * @brief 自定义角色移动组件，为 ECustomMovementMode 中的每种自定义模式提供 PhysCustom 实现。
 *
 * 架构原则：
 * - CMC 负责"怎么动"（物理模拟、碰撞、网络复制）
 * - ActorComponent（如 ClimbingComponent）负责"什么时候动"（检测、状态切换、输入判定）
 * - 两者通过 MovementMode 通信，CMC 不依赖任何 ActorComponent
 */
UCLASS()
class OPENWORLDARPG_API UOpenWorldARPGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UOpenWorldARPGCharacterMovementComponent();

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	// ==========================================
	// 攀爬移动接口 (供 ClimbingComponent 调用)
	// ==========================================

	/** 设置攀爬输入方向 (ClimbingComponent 每帧调用) */
	void SetClimbInput(float InInputRight, float InInputUp);

	/** 设置当前贴墙法线 (ClimbingComponent 检测到墙壁时调用) */
	void SetClimbWallNormal(const FVector& InNormal);

	/** 清除攀爬输入 (退出攀爬时调用) */
	void ClearClimbInput();

	// ==========================================
	// 攀爬配置
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

	/** 攀爬时的重力衰减 (0=无重力, 1=满重力，用于体力耗尽后下坠) */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbGravityScale = 0.0f;

private:
	/** 攀爬物理模拟 */
	void PhysClimbing(float DeltaTime, int32 Iterations);

	// 攀爬运行时状态
	FVector ClimbWallNormal = FVector::ZeroVector;
	float ClimbInputRight = 0.0f;
	float ClimbInputUp = 0.0f;
};
