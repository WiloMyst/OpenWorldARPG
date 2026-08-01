// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "InputActionValue.h"
#include "Systems/VehicleSystem/Data/WheeledVehicleConfigDataAsset.h"
#include "Systems/VehicleSystem/Components/FlightMovementComponent.h"
#include "FlightVehiclePawnBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USkeletalMeshComponent;

/**
 * 飞行载具基类 Pawn
 * 基于 UFlightMovementComponent 的街机飞行物理，不依赖 Chaos Vehicles
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AFlightVehiclePawnBase : public AVehiclePawnBase
{
    GENERATED_BODY()

public:
    AFlightVehiclePawnBase();

    // --- 组件访问 ---

    UFlightMovementComponent* GetFlightMovement() const { return FlightMovement; }

    // --- 公共接口 override ---

    virtual USkeletalMeshComponent* GetVehicleMesh() const override;
    virtual bool FindSafeExitLocation(FVector& OutLocation) const override;
    virtual void ResetVehicleInputs() override;

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // --- 输入回调 ---

    void InputThrottle(const FInputActionValue& Value);
    void InputPitch(const FInputActionValue& Value);
    void InputRoll(const FInputActionValue& Value);
    void InputYaw(const FInputActionValue& Value);
    void InputBoost(const FInputActionValue& Value);

    // --- 网络: 输入同步 ---

    UFUNCTION(Server, Unreliable, WithValidation)
    void Server_SetFlightInput(float Throttle, float Pitch, float Roll, float Yaw, bool bBoost);

    // --- 动态相机 ---

    void UpdateDynamicCamera(float DeltaTime);

public:
    // --- 动画蓝图数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim")
    float CurrentPitchAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim")
    float CurrentRollAngle = 0.0f;

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Config")
    TObjectPtr<UWheeledVehicleConfigDataAsset> VehicleConfig;

    // --- 输入配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Throttle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Pitch;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Roll;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Yaw;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Boost;

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USkeletalMeshComponent> VehicleMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Movement")
    TObjectPtr<UFlightMovementComponent> FlightMovement;

private:
    /** 打包当前完整输入状态发送到服务端（客户端预测配套） */
    void SendFlightInputToServer();
};
