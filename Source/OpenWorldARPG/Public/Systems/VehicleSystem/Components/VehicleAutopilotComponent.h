// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleAutopilotComponent.generated.h"

class AWheeledVehiclePawnBase;
class UChaosWheeledVehicleMovementComponent;

/** 自动驾驶状态机 */
UENUM(BlueprintType)
enum class EVehicleAutopilotState : uint8
{
    /** 空闲：未激活 */
    Idle            UMETA(DisplayName = "空闲"),
    /** 起步：从静止加速 */
    Starting        UMETA(DisplayName = "起步"),
    /** 巡航：沿路径正常行驶 */
    Cruising        UMETA(DisplayName = "巡航"),
    /** 过弯：检测到弯道，减速 */
    Cornering       UMETA(DisplayName = "过弯"),
    /** 制动：接近目的地，减速 */
    Braking         UMETA(DisplayName = "制动"),
    /** 到达：已到达目的地 */
    Arrived         UMETA(DisplayName = "到达"),
    /** 紧急停车：检测到障碍物 */
    EmergencyStop   UMETA(DisplayName = "紧急停车")
};

/** 到达目的地委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAutopilotArrived);

/**
 * 车辆自动驾驶组件
 *
 * 算法架构：
 * - 转向控制：Pure Pursuit（纯追踪）算法，速度自适应前瞻距离
 * - 速度控制：PID 控制器，带积分抗饱和
 * - 避障检测：前方三射线（左/中/右），紧急刹车 + 减速避让
 * - 弯道限速：基于前方路径曲率估算，v = sqrt(a_lat / curvature)
 *
 * 运行规则：
 * - 仅在服务器端（HasAuthority）运行，控制车辆物理输入
 * - 客户端通过车辆移动复制获得平滑表现
 * - 调用方负责通过 NavMesh 或道路 Spline 生成路径点序列
 * - 组件默认不 Tick，StartAutopilot 时启用，StopAutopilot 时禁用
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UVehicleAutopilotComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVehicleAutopilotComponent();

    // --- 公开接口 ---

    /**
     * 开始沿路径自动驾驶（仅在服务器端有效）
     * @param InPathPoints  路径点序列（世界坐标，至少 2 个点）
     * @param InTargetSpeed 目标巡航速度 (cm/s)，<=0 时使用 DefaultCruiseSpeed
     * @return 是否成功启动
     */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot")
    bool StartAutopilot(const TArray<FVector>& InPathPoints, float InTargetSpeed = -1.0f);

    /** 停止自动驾驶，清空输入 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot")
    void StopAutopilot();

    /** 是否正在自动驾驶 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot", BlueprintPure)
    bool IsAutopilotActive() const { return CurrentState != EVehicleAutopilotState::Idle; }

    /** 获取当前自动驾驶状态 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot", BlueprintPure)
    EVehicleAutopilotState GetAutopilotState() const { return CurrentState; }

    /** 获取剩余路径点数量 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot", BlueprintPure)
    int32 GetRemainingPathPoints() const { return PathPoints.Num() - CurrentPathIndex; }

    // --- 委托 ---

    /** 到达目的地时触发（服务器端） */
    UPROPERTY(BlueprintAssignable, Category = "Vehicle|Autopilot")
    FOnAutopilotArrived OnArrived;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- 核心算法 ---

    /** Pure Pursuit: 计算转向输入 (-1.0 ~ 1.0) */
    float ComputeSteering() const;

    /** PID: 计算油门和刹车输入 (0.0 ~ 1.0) */
    void ComputeThrottleAndBrake(float DeltaTime, float& OutThrottle, float& OutBrake);

    /** 前方三射线避障检测，返回最近障碍距离 (cm)，无障碍返回 -1 */
    float CheckObstacleAhead() const;

    /** 更新前瞻点（沿路径前进，速度自适应距离） */
    void UpdateLookAheadPoint();

    /** 估算前方路径曲率 (rad/cm)，用于弯道限速 */
    float EstimateUpcomingCurvature() const;

    /** 根据曲率计算弯道安全速度 (cm/s) */
    float ComputeCornerSpeed() const;

    /** 状态机更新 */
    void UpdateStateMachine(float DeltaTime, float ObstacleDistance);

    /** 在路径上查找离车辆最近的点索引（只向前搜索，不回头） */
    int32 FindClosestPathIndex() const;

    // --- 辅助 ---

    AWheeledVehiclePawnBase* GetVehicleOwner() const;
    FVector GetVehicleLocation() const;
    FVector GetVehicleForward() const;
    float GetVehicleSpeed() const; // cm/s，正=前进

    // --- 配置: 转向 (Pure Pursuit) ---

    /** 轴距 (cm)，影响转向角计算 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Steering")
    float Wheelbase = 280.0f;

    /** 最小前瞻距离 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Steering")
    float MinLookAheadDistance = 300.0f;

    /** 最大前瞻距离 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Steering")
    float MaxLookAheadDistance = 1500.0f;

    /** 前瞻距离 = 当前速度 × 此系数 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Steering")
    float LookAheadSpeedFactor = 0.8f;

    // --- 配置: 速度 (PID) ---

    /** 默认巡航速度 (cm/s，3000 ≈ 108 km/h) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed")
    float DefaultCruiseSpeed = 3000.0f;

    /** 弯道最大横向加速度 (cm/s²)，限制过弯速度 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed")
    float MaxLateralAcceleration = 500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed|PID")
    float PID_Kp = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed|PID")
    float PID_Ki = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed|PID")
    float PID_Kd = 0.1f;

    /** 积分抗饱和上限 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Speed|PID")
    float PID_IntegralClamp = 2.0f;

    // --- 配置: 避障 ---

    /** 避障射线检测距离 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Obstacle")
    float ObstacleTraceDistance = 1000.0f;

    /** 紧急刹车距离阈值 (cm)，障碍物在此距离内触发紧急停车 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Obstacle")
    float EmergencyBrakeDistance = 300.0f;

    /** 避障射线碰撞通道 */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Obstacle")
    TEnumAsByte<ECollisionChannel> ObstacleTraceChannel = ECC_Visibility;

    // --- 配置: 到达 ---

    /** 开始减速距离 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Arrival")
    float ArrivalDistance = 500.0f;

    /** 完全停止距离 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Arrival")
    float StopDistance = 150.0f;

    /** 判定停车的速度阈值 (cm/s) */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Arrival")
    float StopSpeedThreshold = 50.0f;

    // --- 调试 ---

    /** 是否绘制调试信息（路径、前瞻点、射线） */
    UPROPERTY(EditDefaultsOnly, Category = "Autopilot|Debug")
    bool bDrawDebug = false;

private:
    // --- 运行时状态 ---

    EVehicleAutopilotState CurrentState = EVehicleAutopilotState::Idle;

    /** 路径点序列（世界坐标） */
    TArray<FVector> PathPoints;

    /** 当前跟踪的路径点索引 */
    int32 CurrentPathIndex = 0;

    /** 前瞻点（世界坐标） */
    FVector LookAheadPoint = FVector::ZeroVector;

    /** 前瞻点对应的路径索引 */
    int32 LookAheadIndex = 0;

    /** 当前目标速度 (cm/s) */
    float CurrentTargetSpeed = 3000.0f;

    /** PID 积分累计 */
    float SpeedIntegral = 0.0f;

    /** PID 上一次误差 */
    float PrevSpeedError = 0.0f;

    /** 缓存的载具移动组件 */
    UPROPERTY(Transient)
    TObjectPtr<UChaosWheeledVehicleMovementComponent> CachedVehicleMovement;
};
