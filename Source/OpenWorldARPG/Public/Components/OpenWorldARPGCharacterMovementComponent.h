// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Types/CustomMovementModeTypes.h"
#include "Types/MovementStateTypes.h"
#include "GameplayTagContainer.h"
#include "OpenWorldARPGCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

/** MovementMode 改变时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnMovementModeChangedDelegate, EMovementMode, PrevMode, EMovementMode, NewMode, uint8, PrevCustomMode, uint8, NewCustomMode);

/** 翻越蒙太奇播放请求（动画和物理的职责分离：CMC 只管物理位移，动画由 Character 监听执行） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClimbUpMontageRequested, UAnimMontage*, MontageToPlay);

/**
 * 自定义角色移动组件，为 ECustomMovementMode 提供物理模拟实现。
 *
 * 架构原则：
 * - CMC 管"怎么动"（物理/碰撞/网络复制/状态检测）
 * - GAS 管"意愿和表现"（体力消耗/技能激活/Tag管理）
 * - 严禁在移动逻辑中使用 Timer，所有检测在 CMC 原生物理生命周期中执行
 * - PhysCustom 每帧可能执行多次，内部禁止 FindComponentByClass
 */
UCLASS()
class OPENWORLDARPG_API UOpenWorldARPGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UOpenWorldARPGCharacterMovementComponent();

	// ==========================================
	// 引擎重写 - 生命周期
	// ==========================================

	/**
	 * 重写 UpdateCharacterStateBeforeMovement：构建完美的攀爬检测闭环。
	 *
	 * 引擎执行管线：TickComponent → UpdateCharacterStateBeforeMovement → PerformMovement → UpdateCharacterStateAfterMovement
	 * 在 BeforeMovement 阶段做检测，确保状态切换在物理步进之前完成，
	 * 这样 PhysCustom 就能拿到正确的 CustomMovementMode，避免一帧延迟。
	 */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** 记录 GravityScale/AirControl 的默认值，供非对称重力逻辑恢复使用 */
	virtual void BeginPlay() override;

	// ==========================================
	// 事件委托
	// ==========================================

	/** MovementMode 改变时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementModeChangedDelegate OnMovementModeChangedDelegate;

	/** 翻越蒙太奇播放请求（Character 监听此委托执行蒙太奇播放） */
	UPROPERTY(BlueprintAssignable, Category = "Climbing|Events")
	FOnClimbUpMontageRequested OnClimbUpMontageRequested;

	// ==========================================
	// 初始化
	// ==========================================

	/** 初始化缓存指针（在 BeginPlay 中调用） */
	void CacheOwnerReferences();

	// ==========================================
	// 通用状态查询
	// ==========================================

	bool IsFalling() const;
	bool IsGrounded() const;

	// ==========================================
	// 攀爬接口 (供 GA_ClimbBase 调用)
	// ==========================================

	/**
	 * 主动请求攀爬（按键输入调用）。
	 * 架构：CMC 只做物理环境检测，通过 Event 向上报告，由 GA 决定是否进入攀爬。
	 * 如果环境允许，发送 Event.Movement.TryClimb 事件（携带 HitResult），
	 * GA_ClimbBase 收到事件后决定是否激活，激活后调用 EnterClimb。
	 */
	void TryClimb();

	/** 进入攀爬物理模式（GA_ClimbBase::ActivateAbility 中调用） */
	void EnterClimb(const FHitResult& WallHit);

	/** 退出攀爬物理模式（GA_ClimbBase::EndAbility 中调用，保证 GA 结束物理状态一定退出） */
	void ExitClimb();

	/** 尝试翻越（GA 监听输入后调用） */
	void TryClimbUp();

	/** 翻越完成（由 Character 蒙太奇结束回调通知） */
	void FinishClimbUp();

	bool IsClimbing() const;
	bool IsClimbingUp() const { return bIsClimbingUp; }
	bool IsInCornerTransition() const;

	/** 获取当前墙面法线（脱墙跳方向计算用） */
	FVector GetClimbWallNormal() const { return ClimbWallNormal; }

	/**
	 * 脱墙跳：退出攀爬状态并赋予沿墙面法线向外+向上的速度。
	 * 由 GA_ClimbJump 在脱墙后空翻分支中调用。
	 */
	void DoWallEject();

	/** 执行攀爬环境检测，返回是否检测到可攀爬墙壁及 HitResult */
	bool DetectClimbableWall(FHitResult& OutChestHit);

	/**
	 * 根据墙壁 HitResult 计算 Motion Warping 目标 Transform。
	 * 目标位置 = 墙面击中点 + 法线方向 * TargetWallDistance（保持与墙面的安全距离）
	 * 目标旋转 = 面朝墙壁反法线方向
	 * 此函数供 GA_ClimbBase 在设置 MotionWarping Target 时调用。
	 */
	FTransform CalculateClimbWarpTarget(const FHitResult& WallHit) const;

	// ==========================================
	// 攀爬配置 - 射线检测
	// ==========================================

	/** 攀爬检测射线距离 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbTraceDistance = 60.0f;

	/** 头部 Socket 名称 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	FName HeadSocketName = FName("Head");

	/** 攀爬检测碰撞通道 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	TEnumAsByte<ECollisionChannel> ClimbTraceChannel = ECC_Visibility;

	/** 翻越空间检测胶囊半径 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbUpCheckRadius = 30.0f;

	/** 翻越空间检测胶囊半高 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbUpCheckHalfHeight = 50.0f;

	/** 不可攀爬 Actor 的 Tag */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	FName UnclimbableActorTag = FName("NotClimbable");

	/** 攀爬空间检测胶囊半径 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbSpaceCheckRadius = 20.0f;

	/** 攀爬空间检测胶囊半高 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbSpaceCheckHalfHeight = 90.0f;

	/** 攀爬预测偏移量 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Trace")
	float ClimbPredictOffset = 50.0f;

	// ==========================================
	// 攀爬配置 - 逻辑阈值
	// ==========================================

	/** 进入攀爬的最小输入点积阈值（输入方向与墙法线的点积需小于此值） */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Logic")
	float MinInputDotProduct = -0.2f;

	/** 墙壁法线 Z 分量阈值，超过此值认为墙壁不可攀爬 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Logic")
	float WallNormalZThreshold = 0.2f;

	// --- 攀爬事件 Tag（CMC 通过这些 Tag 向 GAS 报告物理状态变化） ---

	/** 请求尝试攀爬事件 Tag（检测到可攀爬墙壁时发送，GA_ClimbBase 监听此事件激活） */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Events")
	FGameplayTag TryClimbEventTag;

	/** 停止攀爬事件 Tag（检测到落地/离开墙壁时发送，GA_ClimbBase 监听此事件结束） */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Events")
	FGameplayTag ClimbStopEventTag;

	// ==========================================
	// 攀爬配置 - 移动
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbMoveSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float TargetWallDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float ClimbGravityScale = 0.0f;

	/** 墙面吸附 Z 偏移 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float WallSnapZOffset = 0.0f;

	/** 墙面吸附时间 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Movement")
	float WallSnapTime = 0.2f;

	// ==========================================
	// 攀爬配置 - 墙角过渡
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float CornerSweepRadius = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float CornerSweepDistance = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConvexArcRadius = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConcaveSnapDistance = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConvexPositionInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConvexRotationInterpSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConcavePositionInterpSpeed = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float ConcaveRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float CornerArrivalThreshold = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|Corner")
	float CornerRotationArrivalThreshold = 3.0f;

	// ==========================================
	// 攀爬配置 - 攀爬转地面
	// ==========================================

	/** 地面检测射线距离 */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|GroundTransition")
	float GroundDetectDistance = 20.0f;

	/** 最小攀爬离地高度（低于此值不触发落地，防止墙根鬼畜） */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|GroundTransition")
	float MinClimbHeightAboveGround = 50.0f;

	// ==========================================
	// 攀爬配置 - 翻越
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
	float ClimbUpPositionInterpSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
	float ClimbUpRotationInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
	float ClimbUpArrivalThreshold = 3.0f;

	/** 翻越位移偏移（相对于角色位置） */
	UPROPERTY(EditDefaultsOnly, Category = "Climbing|ClimbUp")
	FVector ClimbUpOffset = FVector(80.0f, 0.0f, 70.0f);

	// ==========================================
	// 攀爬配置 - 脱墙跳
	// ==========================================

	/** 脱墙跳水平速度（沿墙面法线向外） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Jump")
	float WallEjectHorizontalSpeed = 500.0f;

	/** 脱墙跳垂直速度（向上） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Jump")
	float WallEjectVerticalSpeed = 600.0f;

	// ==========================================
	// 滑翔接口 (供 GA_GlideBase 调用)
	// ==========================================

	void EnterGlideMode();
	void ExitGlideMode();
	bool IsGliding() const;

	/** 向下射线检测，返回角色脚底到地面的距离(cm)。检测失败返回 -1.0f */
	float GetDistanceToGround() const;

	// ==========================================
	// 滑翔配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideGravityScale = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideAirControl = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	FRotator GlideRotationRate = FRotator(0.0f, 100.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	FVector GlideLaunchVelocity = FVector(0.0f, 0.0f, -200.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMaxHorizontalSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMinDescentSpeed = -200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Gliding|Physics")
	float GlideMaxDescentSpeed = -50.0f;

	/** 启动滑翔所需的最小离地高度(cm)，低于此高度不允许开伞 */
	UPROPERTY(EditDefaultsOnly, Category = "Gliding|StartCondition", meta = (ClampMin = "0.0"))
	float MinGlideStartHeight = 300.0f;

	// ==========================================
	// 游泳接口 (供 GA_SwimBase 调用)
	// ==========================================

	void EnterSwimMode();
	void ExitSwimMode();
	bool IsSwimming() const;
	void EnterFastSwimMode();
	void ExitFastSwimMode();
	bool IsFastSwimming() const;
	const FVector& GetLastSafeLocation() const { return LastSafeLocation; }

	// ==========================================
	// 游泳配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
	float SwimMaxSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
	float FastSwimMaxSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
	float SwimGravityScale = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Physics")
	float SwimAirControl = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|SurfaceSnapping")
	float SurfaceSnapOffset = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|SurfaceSnapping")
	float SurfaceSnapInterpSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Safety")
	float SafeLocationRecordInterval = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim|Detection")
	TEnumAsByte<ECollisionChannel> WaterTraceChannel = ECollisionChannel::ECC_GameTraceChannel1;

	// ==========================================
	// 冲刺接口 (供 GA_SprintBase 调用)
	// ==========================================

	void EnterSprintMode();
	void ExitSprintMode();
	bool IsSprinting() const;

	// ==========================================
	// 冲刺配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Sprint|Physics")
	float SprintMaxWalkSpeed = 1200.0f;

	// ==========================================
	// 慢走接口 (供 GA_WalkBase 调用)
	// ==========================================

	void EnterWalkMode();
	void ExitWalkMode();
	bool IsWalking() const;

	// ==========================================
	// 慢走配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Walk|Physics")
	float WalkMaxWalkSpeed = 200.0f;

	// ==========================================
	// 瞄准接口 (供 GA_AimBase 调用)
	// ==========================================

	void EnterAimMode();
	void ExitAimMode();
	bool IsAiming() const;

	// ==========================================
	// 瞄准配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	float AimMaxWalkSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	bool AimUseControllerRotationYaw = true;

	UPROPERTY(EditDefaultsOnly, Category = "Aim|Physics")
	bool AimOrientRotationToMovement = false;

	// ==========================================
	// 下落状态接口
	// ==========================================

	void SetFallingRotationInterpSpeed(float InSpeed);
	float GetFallingRotationInterpSpeed() const { return FallingRotationInterpSpeed; }

	// ==========================================
	// 地面配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Grounded|Physics")
	FRotator GroundedRotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// ==========================================
	// 跳跃非对称重力配置
	// 上升阶段：高重力（快速上升）+ 允许空中控制 + 较慢转身
	// 下落阶段：高重力（快速下落）+ 剥夺空中控制 + 锁死朝向
	// ==========================================

	/** 上升阶段重力倍率（提高以加快上升速度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
	float RisingGravityScale = 2.0f;

	/** 上升阶段空中控制（允许摇杆调整水平方向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
	float RisingAirControl = 0.8f;

	/** 上升阶段转身速率（较低值避免空中转身过快） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Rising")
	float RisingRotationRate = 200.0f;

	/** 下落阶段重力倍率（产生强烈坠落感） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Falling")
	float FallingGravityScale = 3.0f;

	/** 下落阶段空中控制（完全剥夺水平移动控制权） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Falling")
	float FallingAirControl = 0.0f;

	// ==========================================
	// 下落配置
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
	FRotator FallingRotationRate = FRotator(0.0f, 300.0f, 0.0f);

	// ==========================================
	// 被动运动状态 Tag (CMC 管理，主动行为 Tag 由 GA 的 ActivationOwnedTags 管理)
	// ==========================================

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag AirborneTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag FallingTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag StopGlideEventTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag SwimmingTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag FastSwimmingTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	FGameplayTag MovingTag;

	UPROPERTY(EditDefaultsOnly, Category = "StateTags")
	float MovingSpeedThreshold = 50.0f;

protected:
	// ==========================================
	// 物理模拟 - 攀爬
	// ==========================================

	void PhysClimbing(float DeltaTime, int32 Iterations);
	void PhysClimbingCornerTransition(float DeltaTime, int32 Iterations);
	void PhysClimbUp(float DeltaTime, int32 Iterations);

	// ==========================================
	// 物理模拟 - 滑翔
	// ==========================================

	void PhysGliding(float DeltaTime, int32 Iterations);

	// ==========================================
	// 物理模拟 - 游泳
	// ==========================================

	void PhysSwimming(float DeltaTime, int32 Iterations);

	// ==========================================
	// 攀爬 - 状态检测 (在 UpdateCharacterStateBeforeMovement 中调用)
	// ==========================================

	/** Falling 状态下检测前方墙壁，有则请求攀爬 */
	void CheckFallingToClimb();

	/** Walking 状态下检测前方墙壁，走墙时请求攀爬 */
	void CheckGroundedToClimb();

	/** Climbing 状态下检测攀爬转地面 */
	void CheckClimbToGround();

	/** Climbing 状态下检测墙角过渡 */
	void CheckCornerTransition();

	/** Climbing 状态下检测翻越条件 */
	void CheckAndClimbUp();

	// ==========================================
	// 攀爬 - 状态转换
	// ==========================================

	/** 执行翻越（设置目标位置、切换 ClimbUp 模式、请求蒙太奇播放） */
	void DoClimbUp();

	/** 墙角过渡 - 凸角处理 */
	void HandleConvexCorner(const FHitResult& NewWallHit);

	/** 墙角过渡 - 凹角处理 */
	void HandleConcaveCorner(const FHitResult& NewWallHit);

	// ==========================================
	// 攀爬 - 辅助函数
	// ==========================================

	/** 执行攀爬射线检测（胸部+头部） */
	bool PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit);

	/** 判断墙壁是否可攀爬（法线 Z 分量接近 0） */
	bool IsWallClimbable(const FVector& WallNormal) const;

	/** 计算凸角过渡目标位置 */
	FVector CalculateConvexTargetLocation(const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const;

	/** 计算凹角过渡目标位置 */
	FVector CalculateConcaveTargetLocation(const FVector& NewWallHitLocation, const FVector& NewWallNormal) const;

	/** 清除攀爬运行时状态 */
	void ClearClimbState();

	/** 设置墙面吸附目标 */
	void SetClimbSnapTarget(const FVector& InTargetLocation, const FRotator& InTargetRotation, float InSnapTime);

	// ==========================================
	// 游泳辅助
	// ==========================================

	void ApplySurfaceSnapping(float DeltaTime);
	bool IsStillInWater() const;
	void UpdateLastSafeLocation();

private:
	// ==========================================
	// 缓存指针
	// ==========================================

	UPROPERTY()
	TObjectPtr<ACharacter> CachedOwnerCharacter;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// ==========================================
	// 攀爬运行时状态
	// ==========================================

	FVector ClimbWallNormal = FVector::ZeroVector;

	bool bIsSnappingToWall = false;

	bool bCanTryClimb = false;
	FVector SnapTargetLocation = FVector::ZeroVector;
	FRotator SnapTargetRotation = FRotator::ZeroRotator;
	float SnapInterpSpeed = 10.0f;

	/** 是否正在翻越 */
	bool bIsClimbingUp = false;

	// ==========================================
	// 墙角过渡运行时状态
	// ==========================================

	FVector CornerTargetLocation = FVector::ZeroVector;
	FVector CornerTargetNormal = FVector::ZeroVector;
	ECornerType CornerType = ECornerType::None;

	// ==========================================
	// 翻越运行时状态
	// ==========================================

	FVector ClimbUpTargetLocation = FVector::ZeroVector;
	FRotator ClimbUpTargetRotation = FRotator::ZeroRotator;

	// ==========================================
	// 滑翔运行时状态
	// ==========================================

	float OriginalGravityScale = 1.0f;
	float OriginalAirControl = 0.05f;
	FRotator OriginalRotationRate = FRotator::ZeroRotator;

	// ==========================================
	// 冲刺/慢走运行时状态
	// ==========================================

	float OriginalMaxWalkSpeed = 600.0f;
	bool bIsSprinting = false;
	bool bIsWalking = false;

	// ==========================================
	// 瞄准运行时状态
	// ==========================================

	bool bIsAiming = false;
	bool bOriginalOrientRotationToMovement = true;
	bool bOriginalUseControllerRotationYaw = false;

	// ==========================================
	// 游泳运行时状态
	// ==========================================

	bool bIsFastSwimming = false;
	FVector LastSafeLocation = FVector::ZeroVector;
	float SafeLocationTimer = 0.0f;

	// ==========================================
	// 下落运行时状态
	// ==========================================

	float FallingRotationInterpSpeed = 3.0f;

	// ==========================================
	// 跳跃非对称重力 - 默认值缓存（不加 UPROPERTY）
	// ==========================================

	float DefaultGravityScale;
	float DefaultAirControl;
};
