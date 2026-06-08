// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementStateTypes.generated.h"

/**
 * @brief 角色移动状态枚举，用于 FSM 驱动。
 * 每个状态对应独立的行为逻辑和可配参数。
 */
UENUM(BlueprintType)
enum class EMovementState : uint8
{
	None = 0,
	Grounded,   // 地面移动 (Walk/Run)
	Falling,    // 下落/空中
	Climbing,   // 攀爬
	Gliding,    // 滑翔
	Swimming,   // 游泳
	CornerTransition  // 墙角过渡 (阳角/阴角平滑过渡中)
};

/**
 * @brief 墙角过渡类型
 */
UENUM(BlueprintType)
enum class ECornerType : uint8
{
	None = 0,
	Convex,  // 阳角 (外拐角)
	Concave  // 阴角 (内拐角)
};

/**
 * @brief 下落状态可配参数
 */
USTRUCT(BlueprintType)
struct FFallingStateConfig
{
	GENERATED_BODY()

	/** 下落时的空中控制力 (0~1，越低越难空中变向) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	float AirControl = 0.1f;

	/** 下落时角色左右旋转的插值速率 (越小转向越慢) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	float RotationInterpSpeed = 3.0f;

	/** 下落转攀爬：玩家输入朝向墙面的最小点积阈值 (越负越严格) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float ClimbInputDotThreshold = -0.2f;

	/** 下落转攀爬：墙面法线与水平面的最大角度 (度)，超过此角度不算可攀爬墙面 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float WallNormalMaxVerticalAngle = 45.0f;

	/** 下落转攀爬：射线检测距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float ClimbTraceDistance = 80.0f;
};

/**
 * @brief 攀爬状态可配参数
 */
USTRUCT(BlueprintType)
struct FClimbingStateConfig
{
	GENERATED_BODY()

	/** 攀爬贴墙移动速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float ClimbMoveSpeed = 150.0f;

	/** 攀爬时转身面对墙壁的插值平滑速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float RotationInterpSpeed = 10.0f;

	/** 贴墙移动时期望离墙面的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float TargetWallDistance = 30.0f;

	/** 攀爬时的重力衰减 (0=无重力, 1=满重力) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float GravityScale = 0.0f;

	/** 攀爬转地面：正下方射线检测的触地距离阈值 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|GroundTransition")
	float GroundDetectDistance = 20.0f;

	/** 攀爬预测偏移量乘数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float PredictOffset = 50.0f;

	/** 攀爬周围碰撞检测胶囊体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	float SpaceCheckRadius = 20.0f;

	/** 攀爬周围碰撞检测胶囊体半高 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	float SpaceCheckHalfHeight = 90.0f;

	/** 射线检测通道 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 不可攀爬的 Actor Tag */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Logic")
	FName UnclimbableActorTag = FName("NotClimbable");
};

/**
 * @brief 墙角过渡可配参数
 */
USTRUCT(BlueprintType)
struct FCornerTransitionConfig
{
	GENERATED_BODY()

	/** 墙角检测：侧边射线距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Detection")
	float SideTraceDistance = 50.0f;

	/** 墙角检测：前方射线距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Detection")
	float ForwardTraceDistance = 60.0f;

	/** 阳角过渡：环绕弧线的半径 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexArcRadius = 30.0f;

	/** 阳角过渡：位置插值速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexPositionInterpSpeed = 5.0f;

	/** 阳角过渡：旋转插值速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexRotationInterpSpeed = 8.0f;

	/** 阴角过渡：位置插值速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcavePositionInterpSpeed = 6.0f;

	/** 阴角过渡：旋转插值速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcaveRotationInterpSpeed = 10.0f;

	/** 阴角过渡：吸附到新墙面的偏移距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcaveSnapDistance = 30.0f;

	/** 墙角过渡完成阈值：位置距离小于此值视为到达 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|General")
	float ArrivalThreshold = 2.0f;

	/** 墙角过渡完成阈值：角度差小于此值视为到达 (度) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|General")
	float RotationArrivalThreshold = 3.0f;
};

/**
 * @brief 滑翔状态可配参数
 */
USTRUCT(BlueprintType)
struct FGlidingStateConfig
{
	GENERATED_BODY()

	/** 滑翔时的重力缩放 (0=无重力下坠, 1=正常重力) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float GravityScale = 0.0f;

	/** 滑翔时的空中控制力 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float AirControl = 0.8f;

	/** 滑翔时的旋转速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	FRotator RotationRate = FRotator(0.0f, 0.0f, 100.0f);

	/** 进入滑翔时的初始弹射速度 (世界空间) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	FVector LaunchVelocity = FVector(0.0f, 0.0f, -200.0f);

	/** 滑翔最大水平速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MaxHorizontalSpeed = 600.0f;

	/** 滑翔最小下落速度 (cm/s，负值表示向下) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MinDescentSpeed = -200.0f;

	/** 滑翔最大下落速度 (cm/s，负值表示向下) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MaxDescentSpeed = -50.0f;
};

/**
 * @brief 所有移动状态的可配参数集合
 */
USTRUCT(BlueprintType)
struct FMovementStateConfigs
{
	GENERATED_BODY()

	/** 地面移动旋转速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded")
	float GroundedRotationRate = 500.0f;

	/** 下落状态参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	FFallingStateConfig FallingConfig;

	/** 攀爬状态参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing")
	FClimbingStateConfig ClimbingConfig;

	/** 墙角过渡参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CornerTransition")
	FCornerTransitionConfig CornerConfig;

	/** 滑翔状态参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding")
	FGlidingStateConfig GlidingConfig;
};
