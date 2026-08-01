// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "Systems/VehicleSystem/Components/WaterMovementComponent.h"
#include "InputActionValue.h"
#include "WaterVehiclePawnBase.generated.h"

class USkeletalMeshComponent;
class USceneComponent;
class UInputAction;

/**
 * 水面载具基类 Pawn（轮船/航母）
 * 基于 UWaterMovementComponent 的浮力 + 波浪物理
 *
 * 架构特点：
 * - TiltPivot 模式：Root 保持竖直，Mesh 承载波浪俯仰/横滚
 * - 甲板可行走：Root 碰撞体随船移动，站在上面的 Actor 自动跟随
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AWaterVehiclePawnBase : public AVehiclePawnBase
{
    GENERATED_BODY()

public:
    AWaterVehiclePawnBase();

    // --- 组件访问 ---

    UWaterMovementComponent* GetWaterMovement() const { return WaterMovement; }

    // --- 公共接口 override ---

    virtual USkeletalMeshComponent* GetVehicleMesh() const override;
    virtual bool FindSafeExitLocation(FVector& OutLocation) const override;
    virtual void ResetVehicleInputs() override;

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // --- 输入回调 ---

    void InputThrottle(const FInputActionValue& Value);
    void InputRudder(const FInputActionValue& Value);

    // --- 网络 RPC ---

    UFUNCTION(Server, Unreliable, WithValidation)
    void Server_SetWaterInput(float Throttle, float Rudder);

    // --- 视觉/相机 ---

    void UpdateMeshTilt(float DeltaTime);
    void UpdateDynamicCamera(float DeltaTime);

    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USceneComponent> TiltPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Mesh")
    TObjectPtr<USkeletalMeshComponent> VehicleMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Movement")
    TObjectPtr<UWaterMovementComponent> WaterMovement;

    // --- 输入配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Throttle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_Rudder;
};
