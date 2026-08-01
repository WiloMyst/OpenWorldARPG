// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Pawns/HoverVehiclePawnBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"

// ============================================================================
// 构造函数
// ============================================================================

AHoverVehiclePawnBase::AHoverVehiclePawnBase()
{
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // 根节点（不旋转，保持竖直）
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // 倾斜挂点（Mesh 挂在此节点下，倾斜仅影响视觉）
    TiltPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TiltPivot"));
    TiltPivot->SetupAttachment(RootComponent);

    // 骨骼网格体
    VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetupAttachment(TiltPivot);
    VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));

    // 弹簧臂（俯视角，适合无人机视角）
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 500.0f;
    SpringArm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraLagSpeed = 8.0f;
    SpringArm->CameraRotationLagSpeed = 8.0f;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;

    // 摄像机
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;

    // 悬停移动组件
    HoverMovement = CreateDefaultSubobject<UHoverMovementComponent>(TEXT("HoverMovement"));
    HoverMovement->SetUpdatedComponent(RootComponent);
}

// --- 公共接口 override ---

USkeletalMeshComponent* AHoverVehiclePawnBase::GetVehicleMesh() const
{
    return VehicleMesh;
}

void AHoverVehiclePawnBase::ResetVehicleInputs()
{
    if (HoverMovement)
    {
        HoverMovement->ClearInput();
    }
}

bool AHoverVehiclePawnBase::FindSafeExitLocation(FVector& OutLocation) const
{
    const FVector RightOffset = GetActorRightVector() * 250.0f;
    const FVector DownOffset = FVector(0.0f, 0.0f, -50.0f);
    FVector CandidateLocation = GetActorLocation() + RightOffset + DownOffset;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(
        Hit, CandidateLocation, CandidateLocation - FVector(0, 0, 5000.0f),
        ECC_Visibility, Params))
    {
        OutLocation = Hit.Location + FVector(0, 0, 100.0f);
        return true;
    }

    OutLocation = GetActorLocation() + RightOffset;
    return true;
}

// ============================================================================
// 生命周期
// ============================================================================

void AHoverVehiclePawnBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    CurrentYawAngle = GetActorRotation().Yaw;

    if (HoverMovement)
    {
        CurrentSpeedKPH = HoverMovement->GetCurrentHorizontalSpeed() * 0.036f;
    }

    UpdateMeshTilt(DeltaTime);
    UpdateDynamicCamera(DeltaTime);
}

void AHoverVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Forward)    EIC->BindAction(IA_Forward,    ETriggerEvent::Triggered, this, &AHoverVehiclePawnBase::InputForward);
        if (IA_Strafe)     EIC->BindAction(IA_Strafe,     ETriggerEvent::Triggered, this, &AHoverVehiclePawnBase::InputStrafe);
        if (IA_Throttle)   EIC->BindAction(IA_Throttle,   ETriggerEvent::Triggered, this, &AHoverVehiclePawnBase::InputThrottle);
        if (IA_Yaw)        EIC->BindAction(IA_Yaw,        ETriggerEvent::Triggered, this, &AHoverVehiclePawnBase::InputYaw);
        if (IA_Precision)  EIC->BindAction(IA_Precision,  ETriggerEvent::Triggered, this, &AHoverVehiclePawnBase::InputPrecision);
    }
}

// ============================================================================
// 输入回调
// ============================================================================

void AHoverVehiclePawnBase::SendHoverInputToServer()
{
    if (!HoverMovement || HasAuthority()) return;
    Server_SetHoverInput(
        HoverMovement->GetForwardInput(),
        HoverMovement->GetStrafeInput(),
        HoverMovement->GetThrottleInput(),
        HoverMovement->GetYawInput(),
        HoverMovement->IsPrecisionMode());
}

void AHoverVehiclePawnBase::InputForward(const FInputActionValue& Value)
{
    if (!HoverMovement) return;
    HoverMovement->SetForwardInput(Value.Get<float>());
    SendHoverInputToServer();
}

void AHoverVehiclePawnBase::InputStrafe(const FInputActionValue& Value)
{
    if (!HoverMovement) return;
    HoverMovement->SetStrafeInput(Value.Get<float>());
    SendHoverInputToServer();
}

void AHoverVehiclePawnBase::InputThrottle(const FInputActionValue& Value)
{
    if (!HoverMovement) return;
    HoverMovement->SetThrottleInput(Value.Get<float>());
    SendHoverInputToServer();
}

void AHoverVehiclePawnBase::InputYaw(const FInputActionValue& Value)
{
    if (!HoverMovement) return;
    HoverMovement->SetYawInput(Value.Get<float>());
    SendHoverInputToServer();
}

void AHoverVehiclePawnBase::InputPrecision(const FInputActionValue& Value)
{
    if (!HoverMovement) return;
    HoverMovement->SetPrecisionMode(Value.Get<bool>());
    SendHoverInputToServer();
}

// ============================================================================
// 网络 RPC
// ============================================================================

void AHoverVehiclePawnBase::Server_SetHoverInput_Implementation(
    float Forward, float Strafe, float Throttle, float Yaw, bool bPrecision)
{
    // 只有控制此载具的玩家才能设置输入
    if (!GetController()) return;
    if (HoverMovement)
    {
        HoverMovement->SetForwardInput(Forward);
        HoverMovement->SetStrafeInput(Strafe);
        HoverMovement->SetThrottleInput(Throttle);
        HoverMovement->SetYawInput(Yaw);
        HoverMovement->SetPrecisionMode(bPrecision);
    }
}

bool AHoverVehiclePawnBase::Server_SetHoverInput_Validate(
    float Forward, float Strafe, float Throttle, float Yaw, bool bPrecision)
{
    // 防劫持：只有控制此载具的玩家才能发送输入
    return GetController() != nullptr;
}

// ============================================================================
// 视觉倾斜
// ============================================================================

void AHoverVehiclePawnBase::UpdateMeshTilt(float DeltaTime)
{
    if (!HoverMovement || !TiltPivot) return;

    const float TargetPitch = HoverMovement->GetTiltPitch();
    const float TargetRoll = HoverMovement->GetTiltRoll();

    FRotator CurrentTilt = TiltPivot->GetRelativeRotation();
    CurrentTilt.Pitch = FMath::FInterpTo(CurrentTilt.Pitch, TargetPitch, DeltaTime, 8.0f);
    CurrentTilt.Roll = FMath::FInterpTo(CurrentTilt.Roll, TargetRoll, DeltaTime, 8.0f);

    TiltPivot->SetRelativeRotation(CurrentTilt);
}

// ============================================================================
// 动态相机
// ============================================================================

void AHoverVehiclePawnBase::UpdateDynamicCamera(float DeltaTime)
{
    if (!FollowCamera || !HoverMovement) return;

    const float SpeedRatio = FMath::Clamp(
        HoverMovement->GetCurrentHorizontalSpeed() / 1000.0f, 0.0f, 1.0f);
    const float TargetFOV = FMath::Lerp(BaseFOV, BaseFOV + 10.0f, SpeedRatio);

    FollowCamera->FieldOfView = FMath::FInterpTo(
        FollowCamera->FieldOfView, TargetFOV, DeltaTime, 3.0f);
}
