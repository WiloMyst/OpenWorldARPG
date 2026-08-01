// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Pawns/WaterVehiclePawnBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"

// ============================================================================
// 构造函数
// ============================================================================

AWaterVehiclePawnBase::AWaterVehiclePawnBase()
{
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // 根节点（碰撞体，保持竖直，甲板可行走）
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // 倾斜挂点（波浪俯仰/横滚，仅视觉）
    TiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TiltPivot"));
    TiltPivot->SetupAttachment(RootComponent);

    // 骨骼网格体
    VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetupAttachment(TiltPivot);
    VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));

    // 弹簧臂（高位俯视，适合大船）
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 1200.0f;
    SpringArm->SetRelativeRotation(FRotator(-35.0f, 0.0f, 0.0f));
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraLagSpeed = 5.0f;
    SpringArm->CameraRotationLagSpeed = 5.0f;
    // 船相机：跟随偏航，不跟随俯仰/横滚
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;

    // 摄像机
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;

    // 水面移动组件
    WaterMovement = CreateDefaultSubobject<UWaterMovementComponent>(TEXT("WaterMovement"));
    WaterMovement->SetUpdatedComponent(RootComponent);
}

// --- 公共接口 override ---

USkeletalMeshComponent* AWaterVehiclePawnBase::GetVehicleMesh() const
{
    return VehicleMesh;
}

bool AWaterVehiclePawnBase::FindSafeExitLocation(FVector& OutLocation) const
{
    // 在甲板上找下车点
    const FVector RightOffset = GetActorRightVector() * 200.0f;
    OutLocation = GetActorLocation() + RightOffset + FVector(0, 0, 100.0f);
    return true;
}

void AWaterVehiclePawnBase::ResetVehicleInputs()
{
    if (WaterMovement)
    {
        WaterMovement->ClearInput();
    }
}

// ============================================================================
// 生命周期
// ============================================================================

void AWaterVehiclePawnBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (WaterMovement)
    {
        CurrentSpeedKPH = WaterMovement->GetCurrentSpeed() * 0.036f;
    }

    UpdateMeshTilt(DeltaTime);
    UpdateDynamicCamera(DeltaTime);
}

void AWaterVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Throttle)     EIC->BindAction(IA_Throttle,     ETriggerEvent::Triggered, this, &AWaterVehiclePawnBase::InputThrottle);
        if (IA_Rudder)       EIC->BindAction(IA_Rudder,       ETriggerEvent::Triggered, this, &AWaterVehiclePawnBase::InputRudder);
    }
}

// ============================================================================
// 输入回调
// ============================================================================

void AWaterVehiclePawnBase::InputThrottle(const FInputActionValue& Value)
{
    if (WaterMovement) WaterMovement->SetThrottleInput(Value.Get<float>());
}

void AWaterVehiclePawnBase::InputRudder(const FInputActionValue& Value)
{
    if (WaterMovement) WaterMovement->SetRudderInput(Value.Get<float>());
}

// ============================================================================
// 网络 RPC
// ============================================================================

void AWaterVehiclePawnBase::Server_SetWaterInput_Implementation(float Throttle, float Rudder)
{
    if (WaterMovement)
    {
        WaterMovement->SetThrottleInput(Throttle);
        WaterMovement->SetRudderInput(Rudder);
    }
}

bool AWaterVehiclePawnBase::Server_SetWaterInput_Validate(float Throttle, float Rudder)
{
    return true;
}

// ============================================================================
// 视觉/相机
// ============================================================================

void AWaterVehiclePawnBase::UpdateMeshTilt(float DeltaTime)
{
    if (!WaterMovement || !TiltPivot) return;

    const float TargetPitch = WaterMovement->GetWavePitch();
    const float TargetRoll = WaterMovement->GetWaveRoll();

    FRotator CurrentTilt = TiltPivot->GetRelativeRotation();
    CurrentTilt.Pitch = FMath::FInterpTo(CurrentTilt.Pitch, TargetPitch, DeltaTime, 5.0f);
    CurrentTilt.Roll = FMath::FInterpTo(CurrentTilt.Roll, TargetRoll, DeltaTime, 5.0f);

    TiltPivot->SetRelativeRotation(CurrentTilt);
}

void AWaterVehiclePawnBase::UpdateDynamicCamera(float DeltaTime)
{
    if (!FollowCamera || !WaterMovement) return;

    const float SpeedRatio = WaterMovement->GetSpeedRatio();
    const float TargetFOV = FMath::Lerp(BaseFOV, BaseFOV + 8.0f, SpeedRatio);

    FollowCamera->FieldOfView = FMath::FInterpTo(
        FollowCamera->FieldOfView, TargetFOV, DeltaTime, 2.0f);
}
