// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "InputActionValue.h"
#include "Interfaces/InteractableInterface.h"
#include "Data/VehicleConfigDataAsset.h"
#include "WheeledVehiclePawnBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputAction;
class UInputMappingContext;

// ============================================================================
// 载具基类 Pawn
// 基于 Chaos Vehicles 插件的 AWheeledVehiclePawn，采用数据驱动设计。
//
// 【架构原则】
// - 载具基类只管物理：油门刹车输入、Chaos 物理运算、相机弹簧臂
// - 绝不在载具内部写 PC->Possess()，所有 Possess/UnPossess 由 Controller 发起 Server RPC
// - 表现与碰撞解耦：玩家上车后不隐藏模型，而是 Attach 到驾驶座 Socket 并关闭移动组件
// - GAS 驱动生命周期：上下车动作由 GA_MountVehicleBase / GA_UnmountVehicleBase 控制
// - 实现交互接口，OnInteract 发送 GameplayEvent 触发上车 GA
// ============================================================================
UCLASS(Abstract)
class OPENWORLDARPG_API AWheeledVehiclePawnBase : public AWheeledVehiclePawn, public IInteractableInterface
{
    GENERATED_BODY()

public:
    AWheeledVehiclePawnBase();

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

    // --- 交互接口实现 ---

    virtual bool CanInteract_Implementation(ACharacter* InstigatorCharacter) const override;
    virtual void OnInteract_Implementation(ACharacter* InstigatorCharacter) override;
    virtual FTransform GetInteractionTargetTransform_Implementation() const override;

    // --- 驾驶员管理 ---

    /** 当前驾驶员（网络同步） */
    UPROPERTY(ReplicatedUsing = OnRep_Driver, BlueprintReadOnly, Category = "Vehicle|Possession",
        meta = (DisplayName = "当前驾驶员"))
    ACharacter* Driver = nullptr;

    // --- 供 GA / Controller 调用的公开接口 ---

    /** 查找安全下车位置（供 GA_UnmountVehicleBase 调用） */
    bool FindSafeExitLocation(FVector& OutLocation) const;

    /** 重置载具输入（供 Controller 在下车时调用） */
    void ResetVehicleInputs();

    /** 获取驾驶座 Socket 名称 */
    FName GetDriverSeatSocketName() const { return DriverSeatSocketName; }

    /** 获取下车事件 Tag */
    FGameplayTag GetUnmountVehicleEventTag() const { return UnmountVehicleEventTag; }

    // --- 动画蓝图数据 ---

    /** 当前方向盘转角（插值后），供双手 IK 使用 */
    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim",
        meta = (DisplayName = "当前转向角度"))
    float CurrentSteeringAngle = 0.0f;

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
    virtual void PawnClientRestart() override;
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

    // --- 网络同步回调 ---

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

    // --- 上下车配置 ---

    /** 驾驶员座位 Socket 名称（角色 Attach 到此 Socket） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession",
        meta = (DisplayName = "驾驶座 Socket"))
    FName DriverSeatSocketName = FName("DriverSeat");

    /** 交互吸附点 Socket 名称（Motion Warping 目标位置，如车门外侧） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession",
        meta = (DisplayName = "交互吸附点 Socket"))
    FName InteractionSocketName = FName("InteractionPoint");

    // --- GAS 事件 Tags ---

    /** 上车事件 Tag（OnInteract 时发送给角色，激活 GA_MountVehicleBase） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag MountVehicleEventTag;

    /** 下车事件 Tag（车内按下车键时发送给角色，激活 GA_UnmountVehicleBase） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag UnmountVehicleEventTag;

private:

    /** 缓存的 Wheeled 移动组件指针（避免每次 Cast） */
    UPROPERTY(Transient)
    TObjectPtr<UChaosWheeledVehicleMovementComponent> CachedWheeledMovement;

    /** 相机插值当前 Alpha（0=基础，1=最大） */
    float CameraAlpha = 0.0f;
};
