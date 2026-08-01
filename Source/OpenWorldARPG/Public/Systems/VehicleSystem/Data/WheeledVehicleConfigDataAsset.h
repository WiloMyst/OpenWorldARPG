// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WheeledVehicleConfigDataAsset.generated.h"

class UCurveFloat;
class UCurveTable;

// ============================================================================
// 齿轮比配置：每个挡位的传动比与效率
// ============================================================================
USTRUCT(BlueprintType)
struct FGearConfig
{
    GENERATED_BODY()

    /** 该挡位的齿轮比（>1 增扭矩降转速，<1 反之） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gear",
        meta = (DisplayName = "齿轮比", ClampMin = "0.01"))
    float Ratio = 1.0f;

    /** 该挡位的传动效率 (0~1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gear",
        meta = (DisplayName = "传动效率", ClampMin = "0.0", ClampMax = "1.0"))
    float Efficiency = 0.9f;
};

// ============================================================================
// 悬挂配置：每个车轮的悬挂参数
// ============================================================================
USTRUCT(BlueprintType)
struct FSuspensionConfig
{
    GENERATED_BODY()

    /** 悬挂弹簧刚度 (N/m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "弹簧刚度", ClampMin = "0.0"))
    float SpringRate = 350000.0f;

    /** 悬挂阻尼系数 (N·s/m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "阻尼系数", ClampMin = "0.0"))
    float DampingRate = 5000.0f;

    /** 悬挂最大压缩量 (cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "最大压缩量", ClampMin = "0.0"))
    float MaxDrop = 15.0f;

    /** 悬挂最大拉伸量 (cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "最大拉伸量", ClampMin = "0.0"))
    float MaxRaise = 15.0f;

    /** 悬挂预压缩量（影响车身高度） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "预压缩量", ClampMin = "0.0"))
    float PreLoad = 0.0f;
};

// ============================================================================
// 轮胎配置：每个车轮的轮胎物理参数
// ============================================================================
USTRUCT(BlueprintType)
struct FTireConfig
{
    GENERATED_BODY()

    /** 轮胎侧偏刚度（决定转向抓地力） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tire",
        meta = (DisplayName = "侧偏刚度"))
    float LateralStiffness = 1000.0f;

    /** 轮胎纵向刚度（决定加速/制动抓地力） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tire",
        meta = (DisplayName = "纵向刚度"))
    float LongitudinalStiffness = 1000.0f;

    /** 摩擦力缩放（1.0 = 默认摩擦） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tire",
        meta = (DisplayName = "摩擦力缩放", ClampMin = "0.0"))
    float FrictionScale = 1.0f;

    /** 侧滑摩擦力缩放 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tire",
        meta = (DisplayName = "侧滑摩擦力缩放", ClampMin = "0.0"))
    float LateralSlipFrictionScale = 1.0f;
};

// ============================================================================
// 载具配置数据资产：数据驱动的核心，所有物理参数集中管理
// ============================================================================
UCLASS(BlueprintType)
class OPENWORLDARPG_API UWheeledVehicleConfigDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // --- 车辆身份 ---

    /** 载具名称 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity",
        meta = (DisplayName = "载具名称"))
    FText VehicleName;

    /** 载具类型标签（如 Vehicle.Car / Vehicle.Motorcycle） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity",
        meta = (DisplayName = "载具类型Tag"))
    FGameplayTag VehicleTypeTag;

    // --- 车身物理 ---

    /** 车辆质量 (kg) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chassis",
        meta = (DisplayName = "车辆质量(kg)", ClampMin = "1.0"))
    float Mass = 1500.0f;

    /** 质心高度偏移 (cm)，负值降低重心 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chassis",
        meta = (DisplayName = "质心高度偏移(cm)"))
    float COMHeightOffset = -10.0f;

    /** 车身拖拽系数 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chassis",
        meta = (DisplayName = "拖拽系数", ClampMin = "0.0"))
    float DragCoefficient = 0.3f;

    /** 下压力系数（高速时增加抓地力） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chassis",
        meta = (DisplayName = "下压力系数", ClampMin = "0.0"))
    float DownforceCoefficient = 0.0f;

    // --- 引擎 ---

    /** 引擎最大转矩曲线：X轴=引擎转速(RPM)，Y轴=转矩(N·m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine",
        meta = (DisplayName = "最大转矩曲线"))
    TObjectPtr<UCurveFloat> TorqueCurve;

    /** 怠速转速 (RPM) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine",
        meta = (DisplayName = "怠速转速(RPM)", ClampMin = "0.0"))
    float IdleRPM = 800.0f;

    /** 最大转速 (RPM) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine",
        meta = (DisplayName = "最大转速(RPM)", ClampMin = "0.0"))
    float MaxRPM = 7000.0f;

    /** 引擎制动效果系数 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine",
        meta = (DisplayName = "引擎制动系数", ClampMin = "0.0"))
    float EngineBrakeEffect = 0.2f;

    // --- 变速箱 ---

    /** 各挡位齿轮比配置（索引0=倒挡，索引1=空挡，索引2起=1挡、2挡...） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "齿轮比配置"))
    TArray<FGearConfig> GearRatios = {
        { -2.0f, 0.9f },   // 倒挡
        {  0.0f, 0.9f },   // 空挡
        {  3.5f, 0.9f },   // 1挡
        {  2.5f, 0.9f },   // 2挡
        {  1.8f, 0.9f },   // 3挡
        {  1.3f, 0.9f },   // 4挡
        {  1.0f, 0.9f },   // 5挡
        {  0.8f, 0.9f }    // 6挡
    };

    /** 最终传动比（差速器减速比） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "最终传动比", ClampMin = "0.01"))
    float FinalDriveRatio = 3.5f;

    /** 是否为自动挡 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "自动挡"))
    bool bAutomaticTransmission = true;

    /** 自动挡换挡延迟 (s) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "换挡延迟(s)", ClampMin = "0.0", EditCondition = "bAutomaticTransmission"))
    float GearAutoBoxLatency = 0.5f;

    /** 升挡转速 (RPM) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "升挡转速(RPM)", ClampMin = "0.0", EditCondition = "bAutomaticTransmission"))
    float UpShiftRPM = 5500.0f;

    /** 降挡转速 (RPM) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Transmission",
        meta = (DisplayName = "降挡转速(RPM)", ClampMin = "0.0", EditCondition = "bAutomaticTransmission"))
    float DownShiftRPM = 2500.0f;

    // --- 转向 ---

    /** 最大转向角度 (度) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering",
        meta = (DisplayName = "最大转向角度(度)", ClampMin = "0.0", ClampMax = "90.0"))
    float MaxSteeringAngle = 70.0f;

    /** 转向速度（度/秒），越大转向越灵敏 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering",
        meta = (DisplayName = "转向速度(度/s)", ClampMin = "0.0"))
    float SteeringSpeed = 120.0f;

    /** 速度相关转向缩放曲线：X轴=速度(km/h)，Y轴=转向缩放(0~1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering",
        meta = (DisplayName = "速度-转向缩放曲线"))
    TObjectPtr<UCurveFloat> SpeedSteeringCurve;

    // --- 悬挂 ---

    /** 悬挂配置（应用于所有车轮，可在子类中按车轮覆盖） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suspension",
        meta = (DisplayName = "悬挂配置"))
    FSuspensionConfig SuspensionSetup;

    // --- 轮胎 ---

    /** 轮胎配置（应用于所有车轮，可在子类中按车轮覆盖） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tire",
        meta = (DisplayName = "轮胎配置"))
    FTireConfig TireSetup;

    // --- 制动 ---

    /** 最大制动力矩 (N·m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake",
        meta = (DisplayName = "最大制动力矩(N·m)", ClampMin = "0.0"))
    float MaxBrakeTorque = 3000.0f;

    /** 手刹力矩 (N·m) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake",
        meta = (DisplayName = "手刹力矩(N·m)", ClampMin = "0.0"))
    float HandbrakeTorque = 6000.0f;

    // --- 相机 ---

    /** 相机基础臂长 (cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera",
        meta = (DisplayName = "基础臂长(cm)", ClampMin = "0.0"))
    float BaseArmLength = 600.0f;

    /** 相机最大臂长 (cm)（高速时插值目标） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera",
        meta = (DisplayName = "最大臂长(cm)", ClampMin = "0.0"))
    float MaxArmLength = 1000.0f;

    /** 基础FOV */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera",
        meta = (DisplayName = "基础FOV", ClampMin = "20.0", ClampMax = "120.0"))
    float BaseFOV = 90.0f;

    /** 最大FOV（高速时插值目标） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera",
        meta = (DisplayName = "最大FOV", ClampMin = "20.0", ClampMax = "120.0"))
    float MaxFOV = 105.0f;

    /** 速度-FOV/臂长缩放曲线：X轴=速度(km/h)，Y轴=插值Alpha(0~1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera",
        meta = (DisplayName = "速度-相机缩放曲线"))
    TObjectPtr<UCurveFloat> SpeedCameraCurve;

    // --- 上下车 ---

    /** 驾驶员挂载的骨骼Socket名 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession",
        meta = (DisplayName = "驾驶员Socket名"))
    FName DriverSocketName = FName("DriverSeat");

    /** 下车时相对车辆的偏移位置 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession",
        meta = (DisplayName = "下车偏移位置"))
    FVector ExitOffset = FVector(0.0f, -200.0f, 100.0f);

    /** 下车安全检测半径 (cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession",
        meta = (DisplayName = "下车安全检测半径(cm)", ClampMin = "0.0"))
    float ExitCheckRadius = 10.0f;

    /** 最大上车距离 (cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Possession",
        meta = (DisplayName = "最大上车距离(cm)", ClampMin = "0.0"))
    float MaxEnterDistance = 300.0f;

    // --- 辅助方法 ---

    /** 获取该资产类型标识 */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("VehicleConfig", GetFName());
    }
};
