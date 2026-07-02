// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Vehicles/WheeledVehiclePawnBase.h"
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
    bReplicates = true;
    SetReplicatingMovement(true);

    PrimaryActorTick.bCanEverTick = true;

    if (USkeletalMeshComponent* VehicleMesh = GetMesh())
    {
        VehicleMesh->SetSimulatePhysics(true);
        VehicleMesh->SetEnableGravity(true);
        VehicleMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
        VehicleMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetMesh());
    SpringArm->TargetArmLength = 600.0f;
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraRotationLagSpeed = 5.0f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, 150.0f);

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;
}

void AWheeledVehiclePawnBase::BeginPlay()
{
    Super::BeginPlay();

    CachedWheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

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
        if (IA_ExitVehicle)
        {
            EnhancedInput->BindAction(IA_ExitVehicle, ETriggerEvent::Started, this, &AWheeledVehiclePawnBase::InputExitVehicle);
        }
    }
}

void AWheeledVehiclePawnBase::PawnClientRestart()
{
    Super::PawnClientRestart();

    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->AddMappingContext(VehicleIMC, 1);
            }
        }
    }
}

void AWheeledVehiclePawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWheeledVehiclePawnBase, Driver);
}

// --- 交互接口 ---

bool AWheeledVehiclePawnBase::CanInteract_Implementation(ACharacter* InstigatorCharacter) const
{
    return Driver == nullptr && InstigatorCharacter != nullptr;
}

void AWheeledVehiclePawnBase::OnInteract_Implementation(ACharacter* InstigatorCharacter)
{
    if (!InstigatorCharacter || !MountVehicleEventTag.IsValid()) return;

    FGameplayEventData EventData;
    EventData.Instigator = InstigatorCharacter;
    EventData.Target = this;
    EventData.OptionalObject = this;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        InstigatorCharacter, MountVehicleEventTag, EventData);
}

FTransform AWheeledVehiclePawnBase::GetInteractionTargetTransform_Implementation() const
{
    if (USkeletalMeshComponent* VehicleMesh = GetMesh())
    {
        if (VehicleMesh->DoesSocketExist(InteractionSocketName))
        {
            return VehicleMesh->GetSocketTransform(InteractionSocketName);
        }
    }

    FTransform FallbackTransform = GetActorTransform();
    FallbackTransform.AddToTranslation(GetActorForwardVector() * 200.0f);
    return FallbackTransform;
}

// --- 公开接口 ---

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
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetThrottleInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputBrake(const FInputActionValue& Value)
{
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetBrakeInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputSteering(const FInputActionValue& Value)
{
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetSteeringInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputHandbrake(const FInputActionValue& Value)
{
    if (!GetController() || !CachedWheeledMovement) return;
    CachedWheeledMovement->SetHandbrakeInput(Value.Get<bool>());
}

void AWheeledVehiclePawnBase::InputExitVehicle(const FInputActionValue& Value)
{
    if (!GetController()) return;

    FVector ExitLocation;
    if (FindSafeExitLocation(ExitLocation))
    {
        if (AOpenWorldPlayerController* PC = Cast<AOpenWorldPlayerController>(GetController()))
        {
            PC->Server_UnPossessVehicle(ExitLocation);
        }
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[BUG] 下车失败：找不到安全下车点！"));
    }
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

// --- 安全下车位置 ---

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

void AWheeledVehiclePawnBase::FellOutOfWorld(const class UDamageType& dmgType)
{
    // 1. 传送逻辑必须只在服务器执行
    if (!HasAuthority()) return;

    // 2. 清除载具输入
    ResetVehicleInputs();

    // 3. 彻底清除 Chaos 载具的物理动量
    // 如果不清除线速度和角速度，载具传送后会带着下坠的惯性砸向地面并弹飞
    if (USkeletalMeshComponent* VehicleMesh = GetMesh())
    {
        VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // 4. 寻找出生点
    AActor* StartSpot = nullptr;
    if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
    {
        // 如果有驾驶员，使用驾驶员的 Controller 寻找出生点；如果没有，尝试获取默认 Controller
        AController* VehController = GetController();
        if (!VehController)
        {
            VehController = GetWorld()->GetFirstPlayerController();
        }
        StartSpot = GM->FindPlayerStart(VehController);
    }

    // 5. 执行传送
    if (StartSpot)
    {
        FRotator SpawnRotation = StartSpot->GetActorRotation();

        // 物理传送（如果有驾驶员 Attach 在车上，会被带着一起传送）
        SetActorLocationAndRotation(
            StartSpot->GetActorLocation(),
            SpawnRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );

        // 6. 重置控制器朝向
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

// --- 网络同步回调 ---

void AWheeledVehiclePawnBase::OnRep_Driver()
{
}
