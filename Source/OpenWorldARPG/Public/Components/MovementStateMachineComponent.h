// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/MovementStateTypes.h"
#include "MovementStateMachineComponent.generated.h"

class APlayerCharacter;
class UCharacterMovementComponent;
class UOpenWorldARPGCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChanged, EMovementState, OldState, EMovementState, NewState);

/**
 * @class UMovementStateMachineComponent
 * @brief 角色移动有限状态机 (FSM) 组件。
 *
 * 职责：
 * - 每帧 Tick 检测当前状态是否需要切换
 * - 管理状态切换的进入/退出逻辑
 * - 将当前状态广播给其他组件 (CMC、ClimbingComponent、AnimInstance)
 * - 持有所有移动状态的可配参数 (FMovementStateConfigs)
 *
 * 设计原则：
 * - FSM 负责"什么时候切换状态"
 * - CMC 负责"怎么动" (物理模拟)
 * - ClimbingComponent 负责"检测什么" (射线检测)
 * - 三者通过 EMovementState 和方法调用通信
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UMovementStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementStateMachineComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==========================================
	// 状态查询
	// ==========================================

	/** 获取当前移动状态 */
	EMovementState GetCurrentState() const { return CurrentState; }

	/** 是否处于攀爬状态 (包括墙角过渡) */
	bool IsClimbing() const;

	/** 是否处于下落状态 */
	bool IsFalling() const;

	/** 是否处于地面状态 */
	bool IsGrounded() const;

	/** 是否正在墙角过渡中 */
	bool IsInCornerTransition() const;

	// ==========================================
	// 状态切换接口 (供 ClimbingComponent 等调用)
	// ==========================================

	/** 请求切换到指定状态 (带验证) */
	bool RequestStateChange(EMovementState NewState);

	/** 强制切换状态 (跳过验证，仅用于紧急情况如 KillZ 重置) */
	void ForceStateChange(EMovementState NewState);

	// ==========================================
	// 配置
	// ==========================================

	/** 所有移动状态的可配参数 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Config")
	FMovementStateConfigs StateConfigs;

	// ==========================================
	// 委托
	// ==========================================

	/** 状态切换时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;

private:
	/** 当前移动状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|State", meta = (AllowPrivateAccess = "true"))
	EMovementState CurrentState = EMovementState::None;

	/** 上一帧的 UE MovementMode (用于检测引擎侧状态变化) */
	uint8 PrevEngineMovementMode = MOVE_None;

	/** 缓存的外部引用 */
	UPROPERTY()
	TObjectPtr<APlayerCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComp;

	UPROPERTY()
	TObjectPtr<UOpenWorldARPGCharacterMovementComponent> CustomMovementComp;

	// ==========================================
	// 状态切换内部逻辑
	// ==========================================

	/** 进入状态时的处理 */
	void OnEnterState(EMovementState State);

	/** 退出状态时的处理 */
	void OnExitState(EMovementState State);

	/** 每帧状态检测：根据引擎 MovementMode 同步 FSM 状态 */
	void DetectStateFromEngine();

	/** 应用当前状态对应的 CMC 参数 */
	void ApplyStateParamsToCMC();
};
