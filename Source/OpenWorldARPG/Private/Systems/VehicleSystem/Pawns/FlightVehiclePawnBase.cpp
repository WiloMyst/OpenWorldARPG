// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Pawns/FlightVehiclePawnBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"

// ============================================================================
// 构造函数
// ============================================================================

AFlightVehiclePawnBase::AFlightVehiclePawnBase()
{
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // 骨骼网格体
    VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));
    VehicleMesh->SetSimulatePhysics(false);
    RootComponent = VehicleMesh;

    // 弹簧臂
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 800.0f;
    SpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraLagSpeed = 10.0f;
    SpringArm->CameraRotationLagSpeed = 10.0f;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;

    // 摄像机
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;

    // 飞行移动组件
    FlightMovement = CreateDefaultSubobject<UFlightMovementComponent>(TEXT("FlightMovement"));
    FlightMovement->SetUpdatedComponent(RootComponent);
}

// --- 公共接口 override ---

USkeletalMeshComponent* AFlightVehiclePawnBase::GetVehicleMesh() const
{
    return VehicleMesh;
}

void AFlightVehiclePawnBase::ResetVehicleInputs()
{
    if (FlightMovement)
    {
        FlightMovement->ClearInput();
    }
}

bool AFlightVehiclePawnBase::FindSafeExitLocation(FVector& OutLocation) const
{
    const FVector RightOffset = GetActorRightVector() * 300.0f;
    const FVector DownOffset = FVector(0.0f, 0.0f, -100.0f);
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

void AFlightVehiclePawnBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const FRotator ActorRot = GetActorRotation();
    CurrentPitchAngle = ActorRot.Pitch;
    CurrentRollAngle = ActorRot.Roll;

    if (FlightMovement)
    {
        CurrentSpeedKPH = FlightMovement->GetCurrentSpeed() * 0.036f;
    }

    UpdateDynamicCamera(DeltaTime);
}

void AFlightVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Throttle)  EIC->BindAction(IA_Throttle,  ETriggerEvent::Triggered, this, &AFlightVehiclePawnBase::InputThrottle);
        if (IA_Pitch)     EIC->BindAction(IA_Pitch,     ETriggerEvent::Triggered, this, &AFlightVehiclePawnBase::InputPitch);
        if (IA_Roll)      EIC->BindAction(IA_Roll,      ETriggerEvent::Triggered, this, &AFlightVehiclePawnBase::InputRoll);
        if (IA_Yaw)       EIC->BindAction(IA_Yaw,       ETriggerEvent::Triggered, this, &AFlightVehiclePawnBase::InputYaw);
        if (IA_Boost)     EIC->BindAction(IA_Boost,     ETriggerEvent::Triggered, this, &AFlightVehiclePawnBase::InputBoost);
    }
}

// ============================================================================
// 输入回调
// ============================================================================

void AFlightVehiclePawnBase::SendFlightInputToServer()
{
    if (!FlightMovement || HasAuthority()) return;
    Server_SetFlightInput(
        FlightMovement->GetThrottleInput(),
        FlightMovement->GetPitchInput(),
        FlightMovement->GetRollInput(),
        FlightMovement->GetYawInput(),
        FlightMovement->IsBoosting());
}

void AFlightVehiclePawnBase::InputThrottle(const FInputActionValue& Value)
{
    if (!FlightMovement) return;
    FlightMovement->SetThrottleInput(Value.Get<float>());
    SendFlightInputToServer();
}

void AFlightVehiclePawnBase::InputPitch(const FInputActionValue& Value)
{
    if (!FlightMovement) return;
    const float Pitch = -Value.Get<float>();
    FlightMovement->SetPitchInput(Pitch);
    SendFlightInputToServer();
}

void AFlightVehiclePawnBase::InputRoll(const FInputActionValue& Value)
{
    if (!FlightMovement) return;
    FlightMovement->SetRollInput(Value.Get<float>());
    SendFlightInputToServer();
}

void AFlightVehiclePawnBase::InputYaw(const FInputActionValue& Value)
{
    if (!FlightMovement) return;
    FlightMovement->SetYawInput(Value.Get<float>());
    SendFlightInputToServer();
}

void AFlightVehiclePawnBase::InputBoost(const FInputActionValue& Value)
{
    if (!FlightMovement) return;
    FlightMovement->SetBoostInput(Value.Get<bool>());
    SendFlightInputToServer();
}

// ============================================================================
// 网络 RPC
// ============================================================================

void AFlightVehiclePawnBase::Server_SetFlightInput_Implementation(
    float Throttle, float Pitch, float Roll, float Yaw, bool bBoost)
{
    // 只有控制此载具的玩家才能设置输入
    if (!GetController()) return;
    if (FlightMovement)
    {
        FlightMovement->SetThrottleInput(Throttle);
        FlightMovement->SetPitchInput(Pitch);
        FlightMovement->SetRollInput(Roll);
        FlightMovement->SetYawInput(Yaw);
        FlightMovement->SetBoostInput(bBoost);
    }
}

bool AFlightVehiclePawnBase::Server_SetFlightInput_Validate(
    float Throttle, float Pitch, float Roll, float Yaw, bool bBoost)
{
    // 防劫持：只有控制此载具的玩家才能发送输入
    return GetController() != nullptr;
}

// ============================================================================
// 动态相机
// ============================================================================

void AFlightVehiclePawnBase::UpdateDynamicCamera(float DeltaTime)
{
    if (!FollowCamera || !FlightMovement) return;

    const float SpeedRatio = FlightMovement->GetSpeedRatio();
    const float TargetFOV = FMath::Lerp(BaseFOV, BaseFOV + 20.0f, SpeedRatio);

    FollowCamera->FieldOfView = FMath::FInterpTo(
        FollowCamera->FieldOfView, TargetFOV, DeltaTime, 3.0f);
}
