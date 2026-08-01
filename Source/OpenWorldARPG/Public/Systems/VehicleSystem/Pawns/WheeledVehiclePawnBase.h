// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "InputActionValue.h"
#include "Systems/VehicleSystem/Data/WheeledVehicleConfigDataAsset.h"
#include "Systems/VehicleSystem/Components/VehicleAutopilotComponent.h"
#include "WheeledVehiclePawnBase.generated.h"

class UChaosWheeledVehicleMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UInputAction;

/** 轮式载具基类 Pawn，基于 Chaos Vehicles，数据驱动配置。 */
UCLASS(Abstract)
class OPENWORLDARPG_API AWheeledVehiclePawnBase : public AVehiclePawnBase
{
    GENERATED_BODY()

public:
    AWheeledVehiclePawnBase();

    // --- 组件访问 ---

    UChaosWheeledVehicleMovementComponent* GetWheeledVehicleMovement() const { return CachedWheeledMovement; }

    // --- 公共接口 override ---

    virtual USkeletalMeshComponent* GetVehicleMesh() const override;
    virtual bool FindSafeExitLocation(FVector& OutLocation) const override;
    virtual void ResetVehicleInputs() override;

    // --- 自动驾驶 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Autopilot")
    TObjectPtr<UVehicleAutopilotComponent> AutopilotComponent;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Autopilot")
    UVehicleAutopilotComponent* GetAutopilotComponent() const { return AutopilotComponent; }

    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Vehicle|Autopilot")
    void Server_StartAutopilot(const TArray<FVector>& PathPoints, float TargetSpeed = -1.0f);

    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Vehicle|Autopilot")
    void Server_StopAutopilot();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
    virtual void UnPossessed() override;

    // --- 初始化 ---

    void ApplyVehicleConfig();
    void InitCameraDefaults();

    // --- 输入回调 ---

    void InputThrottle(const FInputActionValue& Value);
    void InputBrake(const FInputActionValue& Value);
    void InputSteering(const FInputActionValue& Value);
    void InputHandbrake(const FInputActionValue& Value);

    // --- 动态相机 ---

    void UpdateDynamicCamera(float DeltaTime);

    UFUNCTION(Client, Reliable)
    void Client_ResetCameraAndPhysics(FRotator TargetRotation);

public:
    // --- 动画蓝图数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim")
    float CurrentSteeringAngle = 0.0f;

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Config")
    TObjectPtr<UWheeledVehicleConfigDataAsset> VehicleConfig;

    // --- 输入配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Throttle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Brake;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Steering;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Handbrake;

protected:
    // --- 载具网格体（替代原 AWheeledVehiclePawn::GetMesh()）---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USkeletalMeshComponent> VehicleMesh;

    // --- 缓存 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Movement")
    TObjectPtr<UChaosWheeledVehicleMovementComponent> CachedWheeledMovement;

    // --- 运行时状态 ---

    float CameraAlpha = 0.0f;
};
