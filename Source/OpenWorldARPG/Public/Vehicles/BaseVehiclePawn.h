// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "InputActionValue.h"
#include "Data/VehicleConfigDataAsset.h"
#include "BaseVehiclePawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputAction;
class UInputMappingContext;

// ============================================================================
// 载具基类 Pawn
// 基于 Chaos Vehicles 插件的 AWheeledVehiclePawn，采用数据驱动设计。
// 子类只需在蓝图中指定 VehicleConfig 并设置 SkeletalMesh 即可完成配置。
// ============================================================================
UCLASS(Abstract)
class OPENWORLDARPG_API ABaseVehiclePawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ABaseVehiclePawn();

    // --- 组件访问 ---

    /** 获取 Chaos 载具移动组件（Wheeled 版本） */
    UChaosWheeledVehicleMovementComponent* GetWheeledVehicleMovement() const { return CachedWheeledMovement; }

    /** 获取弹簧臂组件 */
    USpringArmComponent* GetSpringArm() const { return SpringArm; }

    /** 获取相机组件 */
    UCameraComponent* GetCamera() const { return FollowCamera; }

    // --- 数据驱动 ---

    /** 载具配置数据资产 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Config",
        meta = (DisplayName = "载具配置数据"))
    TObjectPtr<UVehicleConfigDataAsset> VehicleConfig;

    // --- 上下车接口（供外部调用） ---

    /** 驾驶员进入载具 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Possession",
        meta = (DisplayName = "进入载具"))
    void EnterVehicle(ACharacter* Driver);

    /** 驾驶员离开载具 */
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Possession",
        meta = (DisplayName = "离开载具"))
    void ExitVehicle();

    /** 当前驾驶员（网络同步） */
    UPROPERTY(ReplicatedUsing = OnRep_Driver, BlueprintReadOnly, Category = "Vehicle|Possession",
        meta = (DisplayName = "当前驾驶员"))
    ACharacter* Driver = nullptr;

    // --- 输入配置 ---

    /** 载具输入映射上下文 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "输入映射上下文"))
    TObjectPtr<UInputMappingContext> VehicleIMC;

    /** 油门输入动作 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "油门输入动作"))
    TObjectPtr<UInputAction> IA_Throttle;

    /** 刹车/倒车输入动作 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "刹车/倒车输入动作"))
    TObjectPtr<UInputAction> IA_Brake;

    /** 转向输入动作 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "转向输入动作"))
    TObjectPtr<UInputAction> IA_Steering;

    /** 手刹输入动作 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "手刹输入动作"))
    TObjectPtr<UInputAction> IA_Handbrake;

    /** 下车输入动作 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input",
        meta = (DisplayName = "下车输入动作"))
    TObjectPtr<UInputAction> IA_ExitVehicle;

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- 初始化 ---

    /** 从 VehicleConfig 读取参数并应用到移动组件和车轮实例 */
    void ApplyVehicleConfig();

    /** 初始化相机系统默认值 */
    void InitCameraDefaults();

    // --- 输入回调 ---

    void InputThrottle(const FInputActionValue& Value);
    void InputBrake(const FInputActionValue& Value);
    void InputSteering(const FInputActionValue& Value);
    void InputHandbrake(const FInputActionValue& Value);
    void InputExitVehicle(const FInputActionValue& Value);

    // --- 动态相机 ---

    /** 根据速度动态调整相机 FOV 和臂长 */
    void UpdateDynamicCamera(float DeltaTime);

    // --- 上下车辅助 ---

    /** 检查下车位置是否安全（无碰撞） */
    bool FindSafeExitLocation(FVector& OutLocation) const;

    /** 网络同步：驾驶员变化回调 */
    UFUNCTION()
    void OnRep_Driver();

    // --- 组件 ---

    /** 弹簧臂 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera",
        meta = (DisplayName = "弹簧臂"))
    TObjectPtr<USpringArmComponent> SpringArm;

    /** 跟随相机 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera",
        meta = (DisplayName = "跟随相机"))
    TObjectPtr<UCameraComponent> FollowCamera;

    // --- 运行时状态 ---

    /** 当前速度 (km/h)，缓存以避免每帧重复计算 */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle|State",
        meta = (DisplayName = "当前速度(km/h)"))
    float CurrentSpeedKPH = 0.0f;

private:

    /** 缓存的 Wheeled 移动组件指针（避免每次 Cast） */
    UPROPERTY(Transient)
    TObjectPtr<UChaosWheeledVehicleMovementComponent> CachedWheeledMovement;

    /** 相机插值当前 Alpha（0=基础，1=最大） */
    float CameraAlpha = 0.0f;
};
