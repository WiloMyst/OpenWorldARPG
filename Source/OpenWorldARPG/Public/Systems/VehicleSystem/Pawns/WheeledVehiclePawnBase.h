// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "InputActionValue.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "Systems/VehicleSystem/Data/VehicleConfigDataAsset.h"
#include "WheeledVehiclePawnBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputAction;
class UInputMappingContext;

/** 载具基类 Pawn，基于 Chaos Vehicles，数据驱动配置。 */
UCLASS(Abstract)
class OPENWORLDARPG_API AWheeledVehiclePawnBase : public AWheeledVehiclePawn, public IInteractableInterface
{
    GENERATED_BODY()

public:
    AWheeledVehiclePawnBase();

    // --- 接口实现 (IInteractableInterface) ---

    virtual bool CanInteract_Implementation(ACharacter* InstigatorCharacter) const override;
    virtual void OnInteract_Implementation(ACharacter* InstigatorCharacter) override;
    virtual FTransform GetInteractionTargetTransform_Implementation() const override;

    // --- 组件访问 ---

    UChaosWheeledVehicleMovementComponent* GetWheeledVehicleMovement() const { return CachedWheeledMovement; }
    USpringArmComponent* GetSpringArm() const { return SpringArm; }
    UCameraComponent* GetCamera() const { return FollowCamera; }

    // --- 公开接口 (供 GA / Controller 调用) ---

    bool FindSafeExitLocation(FVector& OutLocation) const;
    void ResetVehicleInputs();
    FName GetDriverSeatSocketName() const { return DriverSeatSocketName; }
    FName GetInteractionSocketName() const { return InteractionSocketName; }
    FGameplayTag GetUnmountVehicleEventTag() const { return UnmountVehicleEventTag; }

protected:
    virtual void BeginPlay() override;
    virtual void PawnClientRestart() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

    // --- 初始化 ---

    void ApplyVehicleConfig();
    void InitCameraDefaults();

    // --- 输入回调 ---

    void InputThrottle(const FInputActionValue& Value);
    void InputBrake(const FInputActionValue& Value);
    void InputSteering(const FInputActionValue& Value);
    void InputHandbrake(const FInputActionValue& Value);
    void InputExitVehicle(const FInputActionValue& Value);

    // --- 动态相机 ---

    void UpdateDynamicCamera(float DeltaTime);

    UFUNCTION(Client, Reliable)
    void Client_ResetCameraAndPhysics(FRotator TargetRotation);

    // --- 网络同步回调 ---

    UFUNCTION()
    void OnRep_Driver();

 public:
    // --- 网络同步状态 ---

    UPROPERTY(ReplicatedUsing = OnRep_Driver, BlueprintReadOnly, Category = "Vehicle|Possession")
    ACharacter* Driver = nullptr;

    // --- 动画蓝图数据 ---

    /** 当前方向盘角度 */
    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim")
    float CurrentSteeringAngle = 0.0f;

    // --- 配置 ---

    /** 载具配置数据资产组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Config")
    TObjectPtr<UVehicleConfigDataAsset> VehicleConfig;

    // --- 输入配置 ---

    /** 载具输入映射上下文组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputMappingContext> VehicleIMC;

    /** 载具输入加速动作组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Throttle;

    /** 载具输入刹车动作组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Brake;

    /** 载具输入方向盘动作组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Steering;

    /** 载具输入手刹动作组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Handbrake;

    /** 载具输入下车动作组件 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_ExitVehicle;

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    // --- 运行时状态 ---

    /** 当前速度（KPH） */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle|State")
    float CurrentSpeedKPH = 0.0f;

    // --- 上下车配置 ---

    /** 载具驱动座位 Socket 名称 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession")
    FName DriverSeatSocketName = FName("DriverSeat");

    /** 载具交互点 Socket 名称 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession")
    FName InteractionSocketName = FName("InteractionPoint");

    // --- GAS 事件 Tags ---

    /** 载具上车事件标签 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag MountVehicleEventTag;

    /** 载具下车事件标签 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag UnmountVehicleEventTag;

private:
    // --- 缓存 ---

    UPROPERTY(Transient)
    TObjectPtr<UChaosWheeledVehicleMovementComponent> CachedWheeledMovement;

    // --- 运行时状态 ---

    float CameraAlpha = 0.0f;
};
