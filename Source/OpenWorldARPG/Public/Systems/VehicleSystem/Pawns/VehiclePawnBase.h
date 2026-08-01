// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "VehiclePawnBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UInputMappingContext;
class UInputAction;
struct FGameplayEventData;

/**
 * 载具公共基类。
 *
 * 【职责】提取 Wheeled/Hover/Flight/Water 四种载具的公共接口与属性，
 * 使 PlayerController 和 GA 可用统一类型操作载具，消除硬编码类型限制。
 *
 * 【设计原则】
 * - IInteractableInterface 统一实现：上车走 GAS 事件流程，不直接设 Driver
 * - 公共属性上提：Driver/SpringArm/FollowCamera/Socket 配置/事件 Tag 等
 * - 公共逻辑上提：IMC 注册/移除、InputExitVehicle、FellOutOfWorld 默认实现
 * - 子类差异通过 virtual 接口隔离：FindSafeExitLocation/ResetVehicleInputs/GetVehicleMesh
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AVehiclePawnBase : public APawn, public IInteractableInterface
{
    GENERATED_BODY()

public:
    AVehiclePawnBase();

    // --- 生命周期 ---

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;
    virtual void UnPossessed() override;
    virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

    // --- 网络复制回调 ---

    /**
     * 服务端 ReplicatedMovement 到达客户端时触发。
     * - 本地控制端 + 预测移动组件：交给移动组件做预测纠错（跳过默认 snap）
     * - 远端/Chaos 载具：走默认行为（snap + 平滑）
     */
    virtual void OnRep_ReplicatedMovement() override;

    // --- IInteractableInterface 统一实现 ---

    virtual bool CanInteract_Implementation(ACharacter* InstigatorCharacter) const override;
    virtual void OnInteract_Implementation(ACharacter* InstigatorCharacter) override;
    virtual FTransform GetInteractionTargetTransform_Implementation() const override;

    // --- 公共虚接口（子类按需 override）---

    /** 查找安全下车位置。默认实现返回 false，子类应 override。 */
    virtual bool FindSafeExitLocation(FVector& OutLocation) const { return false; }

    /** 重置载具输入。子类应 override 以清零各自移动组件的输入。 */
    virtual void ResetVehicleInputs() {}

    /** 获取载具骨骼网格体。子类必须 override 返回各自的 Mesh 组件。 */
    virtual USkeletalMeshComponent* GetVehicleMesh() const { return nullptr; }

    // --- 网络回调 ---

    UFUNCTION()
    virtual void OnRep_Driver() {}

    // --- 纯 getter（基类实现，子类直接复用）---

    FName GetDriverSeatSocketName() const { return DriverSeatSocketName; }
    FName GetInteractionSocketName() const { return InteractionSocketName; }
    FGameplayTag GetMountVehicleEventTag() const { return MountVehicleEventTag; }
    FGameplayTag GetUnmountVehicleEventTag() const { return UnmountVehicleEventTag; }
    USpringArmComponent* GetSpringArm() const { return SpringArm; }
    UCameraComponent* GetCamera() const { return FollowCamera; }
    UInputMappingContext* GetVehicleIMC() const { return VehicleIMC; }
    ACharacter* GetDriver() const { return Driver; }
    void SetDriver(ACharacter* NewDriver) { Driver = NewDriver; }
    float GetMaxEnterDistance() const { return MaxEnterDistance; }

    // --- 网络同步状态 ---

    UPROPERTY(ReplicatedUsing = OnRep_Driver, BlueprintReadOnly, Category = "Vehicle|Possession")
    ACharacter* Driver = nullptr;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- 公共组件（子类在构造函数中创建并挂载）---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    // --- 上下车配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession")
    FName DriverSeatSocketName = FName("DriverSeat");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession")
    FName InteractionSocketName = FName("InteractionPoint");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Possession",
        meta = (ClampMin = "0.0"))
    float MaxEnterDistance = 300.0f;

    // --- GAS 事件 Tags ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag MountVehicleEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Tags")
    FGameplayTag UnmountVehicleEventTag;

    // --- 输入配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputMappingContext> VehicleIMC;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> IA_ExitVehicle;

    // --- 公共运行时状态 ---

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle|State")
    float CurrentSpeedKPH = 0.0f;

    // 统一下车输入回调（基类实现，子类 SetupPlayerInputComponent 调 Super 即可绑定）
    void InputExitVehicle(const FInputActionValue& Value);

    // 基础 FOV，BeginPlay 时从 FollowCamera 记录，子类 UpdateDynamicCamera 可读
    float BaseFOV = 90.0f;
};
