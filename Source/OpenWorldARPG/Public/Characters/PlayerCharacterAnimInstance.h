// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerCharacterAnimInstance.generated.h"

/**
 * @class UPlayerCharacterAnimInstance
 * @brief 玩家角色的自定义动画实例。
 *
 * 核心优化：
 * 1. 实现 NativeThreadSafeUpdateAnimation，将属性解算前置到工作线程，
 *    避免主线程阻塞。这是 UE5 多线程动画更新的标准做法。
 * 2. 所有动画驱动属性在 C++ 中计算，确保 AnimBP 走 Fast Path，
 *    规避蓝图反射开销。
 *
 * 使用方式：在 AnimBP 的父类设置中指定此类替代默认 UAnimInstance。
 */
UCLASS()
class OPENWORLDARPG_API UPlayerCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPlayerCharacterAnimInstance();

	// ==========================================
	// UAnimInstance 接口重写
	// ==========================================

	/** 工作线程安全的属性解算（在 TaskGraph 工作线程执行，不访问 UObject） */
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** 主线程更新（可安全访问 UObject，用于需要主线程上下文的逻辑） */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ==========================================
	// AnimBP 可绑定的属性（Fast Path 兼容）
	// ==========================================

	/** 角色移动速度（2D 水平面） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement", meta = (BlueprintProtected = "true"))
	float GroundSpeed = 0.0f;

	/** 角色是否在空中 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement", meta = (BlueprintProtected = "true"))
	bool bIsFalling = false;

	/** 角色是否在移动 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement", meta = (BlueprintProtected = "true"))
	bool bIsMoving = false;

	/** 移动方向角（-180 ~ 180，用于 Blend Space） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement", meta = (BlueprintProtected = "true"))
	float MovementDirection = 0.0f;

	/** 角色是否在攀爬 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Movement", meta = (BlueprintProtected = "true"))
	bool bIsClimbing = false;

	/** 角色是否在瞄准 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|Combat", meta = (BlueprintProtected = "true"))
	bool bIsAiming = false;

private:
	// ==========================================
	// 线程安全缓存（由 NativeThreadSafeUpdateAnimation 写入）
	// ==========================================

	float ThreadSafe_GroundSpeed = 0.0f;
	bool ThreadSafe_bIsFalling = false;
	bool ThreadSafe_bIsMoving = false;
	float ThreadSafe_MovementDirection = 0.0f;
	bool ThreadSafe_bIsClimbing = false;
};
