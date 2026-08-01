// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Pawns/WheeledVehiclePawnBase.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "Core/PlayerControllers/OpenWorldPlayerController.h"

AWheeledVehiclePawnBase::AWheeledVehiclePawnBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // Chaos 物理级客户端预测：
    // 启用 p.Net.PhysicsPrediction.Enable=1 (DefaultEngine.ini) 后，
    // UChaosWheeledVehicleMovementComponent 会自动创建 UNetworkPhysicsComponent，
    // 基于 Chaos RewindData 做物理级回滚重模拟。
    // 输入通过组件内部 RPC 自动同步，无需手动发送。

    // 载具网格体（物理模拟根组件）
    VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
    VehicleMesh->SetSimulatePhysics(true);
    VehicleMesh->SetEnableGravity(true);
    VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));
    VehicleMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    RootComponent = VehicleMesh;

    // 弹簧臂
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(VehicleMesh);
    SpringArm->TargetArmLength = 600.0f;
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraRotationLagSpeed = 5.0f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, 150.0f);

    // 摄像机
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;

    // Chaos 轮式载具移动组件
    CachedWheeledMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("WheeledVehicleMovement"));
    CachedWheeledMovement->SetUpdatedComponent(VehicleMesh);

    // 自动驾驶组件
    AutopilotComponent = CreateDefaultSubobject<UVehicleAutopilotComponent>(TEXT("AutopilotComponent"));
}

// --- 公共接口 override ---

USkeletalMeshComponent* AWheeledVehiclePawnBase::GetVehicleMesh() const
{
    return VehicleMesh;
}

void AWheeledVehiclePawnBase::ResetVehicleInputs()
{
    if (CachedWheeledMovement)
    {
        CachedWheeledMovement->SetThrottleInput(0.0f);
        CachedWheeledMovement->SetSteeringInput(0.0f);
        CachedWheeledMovement->SetBrakeInput(0.0f);
        CachedWheeledMovement->SetHandbrakeInput(false);
    }
}

bool AWheeledVehiclePawnBase::FindSafeExitLocation(FVector& OutLocation) const
{
    const FVector BaseOffset = VehicleConfig ? VehicleConfig->ExitOffset : FVector(-200.0f, 150.0f, 0.0f);
    const float CheckRadius = VehicleConfig ? VehicleConfig->ExitCheckRadius : 50.0f;

    const FVector CandidateLocation = GetActorTransform().TransformPositionNoScale(BaseOffset);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Driver);

    const FCollisionShape CheckShape = FCollisionShape::MakeSphere(CheckRadius);

    bool bLeftClear = !GetWorld()->OverlapAnyTestByChannel(
        CandidateLocation, FQuat::Identity, ECC_Pawn, CheckShape, QueryParams);

    if (bLeftClear)
    {
        OutLocation = CandidateLocation;
        return true;
    }

    const FVector RightOffset(BaseOffset.X, -BaseOffset.Y, BaseOffset.Z);
    const FVector RightLocation = GetActorTransform().TransformPositionNoScale(RightOffset);

    bool bRightClear = !GetWorld()->OverlapAnyTestByChannel(
        RightLocation, FQuat::Identity, ECC_Pawn, CheckShape, QueryParams);

    if (bRightClear)
    {
        OutLocation = RightLocation;
        return true;
    }

    return false;
}

// --- 生命周期 ---

void AWheeledVehiclePawnBase::BeginPlay()
{
    Super::BeginPlay();

    ApplyVehicleConfig();
    InitCameraDefaults();
}

void AWheeledVehiclePawnBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CachedWheeledMovement)
    {
        CurrentSpeedKPH = CachedWheeledMovement->GetForwardSpeed() * 0.036f;
    }

    UpdateDynamicCamera(DeltaTime);

    // 方向盘转角插值（供动画蓝图双手 IK 使用）
    if (CachedWheeledMovement)
    {
        const float TargetAngle = CachedWheeledMovement->GetSteeringInput() * 70.0f;
        CurrentSteeringAngle = FMath::FInterpTo(CurrentSteeringAngle, TargetAngle, DeltaTime, 10.0f);
    }
}

void AWheeledVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Throttle)
        {
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputThrottle);
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputThrottle);
        }
        if (IA_Brake)
        {
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputBrake);
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputBrake);
        }
        if (IA_Steering)
        {
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputSteering);
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputSteering);
        }
        if (IA_Handbrake)
        {
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputHandbrake);
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputHandbrake);
        }
    }
}

// --- 初始化 ---

void AWheeledVehiclePawnBase::ApplyVehicleConfig()
{
    if (!VehicleConfig || !CachedWheeledMovement) return;

    CachedWheeledMovement->Mass = VehicleConfig->Mass;
    CachedWheeledMovement->DragCoefficient = VehicleConfig->DragCoefficient;
    CachedWheeledMovement->DownforceCoefficient = VehicleConfig->DownforceCoefficient;

    FVehicleEngineConfig& Engine = CachedWheeledMovement->EngineSetup;
    Engine.MaxRPM = VehicleConfig->MaxRPM;
    Engine.EngineIdleRPM = VehicleConfig->IdleRPM;
    Engine.EngineBrakeEffect = VehicleConfig->EngineBrakeEffect;

    if (VehicleConfig->TorqueCurve)
    {
        Engine.TorqueCurve.EditorCurveData = VehicleConfig->TorqueCurve->FloatCurve;
    }

    FVehicleTransmissionConfig& Transmission = CachedWheeledMovement->TransmissionSetup;
    Transmission.bUseAutomaticGears = VehicleConfig->bAutomaticTransmission;
    Transmission.bUseAutoReverse = VehicleConfig->bAutomaticTransmission;
    Transmission.FinalRatio = VehicleConfig->FinalDriveRatio;
    Transmission.ChangeUpRPM = VehicleConfig->UpShiftRPM;
    Transmission.ChangeDownRPM = VehicleConfig->DownShiftRPM;
    Transmission.GearChangeTime = VehicleConfig->GearAutoBoxLatency;

    Transmission.ForwardGearRatios.Reset();
    Transmission.ReverseGearRatios.Reset();

    for (int32 i = 0; i < VehicleConfig->GearRatios.Num(); ++i)
    {
        const FGearConfig& Gear = VehicleConfig->GearRatios[i];
        if (i == 0)
        {
            Transmission.ReverseGearRatios.Add(Gear.Ratio);
            Transmission.TransmissionEfficiency = Gear.Efficiency;
        }
        else if (i >= 2)
        {
            Transmission.ForwardGearRatios.Add(Gear.Ratio);
        }
    }

    FVehicleSteeringConfig& Steering = CachedWheeledMovement->SteeringSetup;
    if (VehicleConfig->SpeedSteeringCurve)
    {
        Steering.SteeringCurve.EditorCurveData = VehicleConfig->SpeedSteeringCurve->FloatCurve;
    }

    const FSuspensionConfig& Susp = VehicleConfig->SuspensionSetup;
    const FTireConfig& Tire = VehicleConfig->TireSetup;

    for (UChaosVehicleWheel* Wheel : CachedWheeledMovement->Wheels)
    {
        if (!Wheel) continue;

        Wheel->SpringRate = Susp.SpringRate;
        Wheel->SuspensionDampingRatio = FMath::Clamp(Susp.DampingRate / 10000.0f, 0.0f, 1.0f);
        Wheel->SuspensionMaxDrop = Susp.MaxDrop;
        Wheel->SuspensionMaxRaise = Susp.MaxRaise;
        Wheel->SpringPreload = Susp.PreLoad;

        Wheel->CorneringStiffness = Tire.LateralStiffness;
        Wheel->FrictionForceMultiplier = Tire.FrictionScale;
        Wheel->SideSlipModifier = Tire.LateralSlipFrictionScale;

        Wheel->MaxBrakeTorque = VehicleConfig->MaxBrakeTorque;
        Wheel->MaxHandBrakeTorque = VehicleConfig->HandbrakeTorque;

        if (Wheel->bAffectedBySteering)
        {
            Wheel->MaxSteerAngle = VehicleConfig->MaxSteeringAngle;
        }
    }

    MaxEnterDistance = VehicleConfig->MaxEnterDistance;
}

void AWheeledVehiclePawnBase::InitCameraDefaults()
{
    if (!VehicleConfig) return;

    SpringArm->TargetArmLength = VehicleConfig->BaseArmLength;
    FollowCamera->FieldOfView = VehicleConfig->BaseFOV;
}

// --- 输入回调 ---

void AWheeledVehiclePawnBase::InputThrottle(const FInputActionValue& Value)
{
    if (AutopilotComponent && AutopilotComponent->IsAutopilotActive()) return;
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetThrottleInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputBrake(const FInputActionValue& Value)
{
    if (AutopilotComponent && AutopilotComponent->IsAutopilotActive()) return;
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetBrakeInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputSteering(const FInputActionValue& Value)
{
    if (AutopilotComponent && AutopilotComponent->IsAutopilotActive()) return;
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetSteeringInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputHandbrake(const FInputActionValue& Value)
{
    if (AutopilotComponent && AutopilotComponent->IsAutopilotActive()) return;
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetHandbrakeInput(Value.Get<bool>());
}

// --- 动态相机 ---

void AWheeledVehiclePawnBase::UpdateDynamicCamera(float DeltaTime)
{
    if (!VehicleConfig || !SpringArm || !FollowCamera) return;

    float TargetAlpha = 0.0f;

    if (VehicleConfig->SpeedCameraCurve)
    {
        TargetAlpha = FMath::Clamp(
            VehicleConfig->SpeedCameraCurve->GetFloatValue(FMath::Abs(CurrentSpeedKPH)),
            0.0f, 1.0f);
    }
    else
    {
        TargetAlpha = FMath::Clamp(FMath::Abs(CurrentSpeedKPH) / 200.0f, 0.0f, 1.0f);
    }

    CameraAlpha = FMath::FInterpTo(CameraAlpha, TargetAlpha, DeltaTime, 3.0f);

    SpringArm->TargetArmLength = FMath::Lerp(VehicleConfig->BaseArmLength, VehicleConfig->MaxArmLength, CameraAlpha);
    FollowCamera->FieldOfView = FMath::Lerp(VehicleConfig->BaseFOV, VehicleConfig->MaxFOV, CameraAlpha);
}

// --- 掉出世界处理 ---

void AWheeledVehiclePawnBase::FellOutOfWorld(const class UDamageType& dmgType)
{
    if (!HasAuthority()) return;

    ResetVehicleInputs();

    if (VehicleMesh)
    {
        VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    AActor* StartSpot = nullptr;
    if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
    {
        AController* VehController = GetController();
        if (!VehController)
        {
            VehController = GetWorld()->GetFirstPlayerController();
        }
        StartSpot = GM->FindPlayerStart(VehController);
    }

    if (StartSpot)
    {
        FRotator SpawnRotation = StartSpot->GetActorRotation();

        SetActorLocationAndRotation(
            StartSpot->GetActorLocation(),
            SpawnRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );

        if (GetController())
        {
            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                PC->SetControlRotation(SpawnRotation);
            }
            Client_ResetCameraAndPhysics(SpawnRotation);
        }
    }
    else
    {
        Super::FellOutOfWorld(dmgType);
    }
}

void AWheeledVehiclePawnBase::Client_ResetCameraAndPhysics_Implementation(FRotator TargetRotation)
{
    if (Controller && IsLocallyControlled())
    {
        Controller->SetControlRotation(TargetRotation);
    }
}

// --- 自动驾驶 RPC ---

void AWheeledVehiclePawnBase::Server_StartAutopilot_Implementation(const TArray<FVector>& PathPoints, float TargetSpeed)
{
    if (!GetController()) return;
    if (AutopilotComponent)
    {
        AutopilotComponent->StartAutopilot(PathPoints, TargetSpeed);
    }
}

bool AWheeledVehiclePawnBase::Server_StartAutopilot_Validate(const TArray<FVector>& PathPoints, float TargetSpeed)
{
    // 只有当前控制载具的玩家可以启动自动驾驶
    return GetController() != nullptr;
}

void AWheeledVehiclePawnBase::Server_StopAutopilot_Implementation()
{
    if (AutopilotComponent)
    {
        AutopilotComponent->StopAutopilot();
    }
}

bool AWheeledVehiclePawnBase::Server_StopAutopilot_Validate()
{
    return GetController() != nullptr;
}

// --- UnPossessed: 玩家下车时停止自动驾驶 ---

void AWheeledVehiclePawnBase::UnPossessed()
{
    Super::UnPossessed();

    if (AutopilotComponent && AutopilotComponent->IsAutopilotActive())
    {
        AutopilotComponent->StopAutopilot();
    }
}
