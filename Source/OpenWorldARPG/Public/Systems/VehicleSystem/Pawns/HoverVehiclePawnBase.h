// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "InputActionValue.h"
#include "Systems/VehicleSystem/Components/HoverMovementComponent.h"
#include "HoverVehiclePawnBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USkeletalMeshComponent;
class USceneComponent;

/**
 * 悬停载具基类 Pawn（四旋翼/无人机）
 * 基于 UHoverMovementComponent 的全向悬停物理
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AHoverVehiclePawnBase : public AVehiclePawnBase
{
    GENERATED_BODY()

public:
    AHoverVehiclePawnBase();

    // --- 组件访问 ---

    UHoverMovementComponent* GetHoverMovement() const { return HoverMovement; }

    // --- 公共接口 override ---

    virtual USkeletalMeshComponent* GetVehicleMesh() const override;
    virtual bool FindSafeExitLocation(FVector& OutLocation) const override;
    virtual void ResetVehicleInputs() override;

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // --- 输入回调 ---

    void InputForward(const FInputActionValue& Value);
    void InputStrafe(const FInputActionValue& Value);
    void InputThrottle(const FInputActionValue& Value);
    void InputYaw(const FInputActionValue& Value);
    void InputPrecision(const FInputActionValue& Value);

    // --- 网络: 输入同步 ---

    UFUNCTION(Server, Unreliable, WithValidation)
    void Server_SetHoverInput(float Forward, float Strafe, float Throttle, float Yaw, bool bPrecision);

    // --- 动态相机 / 视觉倾斜 ---

    void UpdateDynamicCamera(float DeltaTime);
    void UpdateMeshTilt(float DeltaTime);

public:
    // --- 动画/视觉数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Anim")
    float CurrentYawAngle = 0.0f;

    // --- 输入配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Forward;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Strafe;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Throttle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Yaw;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Precision;

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USceneComponent> TiltPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USkeletalMeshComponent> VehicleMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Movement")
    TObjectPtr<UHoverMovementComponent> HoverMovement;

private:
    /** 打包当前完整输入状态发送到服务端（客户端预测配套） */
    void SendHoverInputToServer();
};
