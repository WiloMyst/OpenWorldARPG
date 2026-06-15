// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Vehicles/BaseVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"

// ============================================================================
// 构造函数：初始化组件和默认属性
// ============================================================================
ABaseVehiclePawn::ABaseVehiclePawn()
{
    // 网络同步
    bReplicates = true;
    SetReplicatingMovement(true);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    // --- Chaos 载具移动组件 ---
    // AWheeledVehiclePawn 已在内部创建了 ChaosVehicleMovement，
    // 这里只需确保它存在并做基础配置
    ChaosVehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("ChaosVehicleMovement"));
    ChaosVehicleMovement->SetIsReplicated(true);
    ChaosVehicleMovement->bReverseAsBrake = true;

    // --- 弹簧臂 ---
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetMesh());
    SpringArm->TargetArmLength = 600.0f;
    SpringArm->bEnableCameraLag = true;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraLagSpeed = 8.0f;
    SpringArm->CameraRotationLagSpeed = 5.0f;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;
    // 默认偏移：略高于车顶，略向后
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, 150.0f);

    // --- 跟随相机 ---
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm);
    FollowCamera->FieldOfView = 90.0f;
    FollowCamera->bUsePawnControlRotation = false;
}

// ============================================================================
// BeginPlay：从 DataAsset 读取配置并应用
// ============================================================================
void ABaseVehiclePawn::BeginPlay()
{
    Super::BeginPlay();

    ApplyVehicleConfig();
    InitCameraDefaults();
}

// ============================================================================
// Tick：更新动态相机
// ============================================================================
void ABaseVehiclePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 缓存当前速度 (km/h)
    if (ChaosVehicleMovement)
    {
        CurrentSpeedKPH = ChaosVehicleMovement->GetForwardSpeed() * 0.036f; // cm/s → km/h
    }

    UpdateDynamicCamera(DeltaTime);
}

// ============================================================================
// 输入绑定：Enhanced Input
// ============================================================================
void ABaseVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 油门：1D 轴（W/S 或扳机）
        if (IA_Throttle)
        {
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &ABaseVehiclePawn::InputThrottle);
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Completed, this, &ABaseVehiclePawn::InputThrottle);
        }
        // 刹车/倒车
        if (IA_Brake)
        {
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Triggered, this, &ABaseVehiclePawn::InputBrake);
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Completed, this, &ABaseVehiclePawn::InputBrake);
        }
        // 转向：1D 轴（A/D 或左摇杆）
        if (IA_Steering)
        {
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Triggered, this, &ABaseVehiclePawn::InputSteering);
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Completed, this, &ABaseVehiclePawn::InputSteering);
        }
        // 手刹
        if (IA_Handbrake)
        {
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Triggered, this, &ABaseVehiclePawn::InputHandbrake);
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Completed, this, &ABaseVehiclePawn::InputHandbrake);
        }
        // 下车
        if (IA_ExitVehicle)
        {
            EnhancedInput->BindAction(IA_ExitVehicle, ETriggerEvent::Started, this, &ABaseVehiclePawn::InputExitVehicle);
        }
    }

    // 添加载具输入映射上下文
    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->AddMappingContext(VehicleIMC, 1);
                }
            }
        }
    }
}

// ============================================================================
// 网络同步属性
// ============================================================================
void ABaseVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABaseVehiclePawn, Driver);
}

// ============================================================================
// 从 VehicleConfig 读取参数并应用到 ChaosVehicleMovement
// ============================================================================
void ABaseVehiclePawn::ApplyVehicleConfig()
{
    if (!VehicleConfig || !ChaosVehicleMovement) return;

    // --- 车身物理 ---
    ChaosVehicleMovement->Mass = VehicleConfig->Mass;
    ChaosVehicleMovement->DragCoefficient = VehicleConfig->DragCoefficient;
    ChaosVehicleMovement->DownforceCoefficient = VehicleConfig->DownforceCoefficient;

    // --- 引擎 ---
    ChaosVehicleMovement->MaxEngineRPM = VehicleConfig->MaxRPM;
    ChaosVehicleMovement->EngineIdleRPM = VehicleConfig->IdleRPM;
    ChaosVehicleMovement->EngineBrakeEffect = VehicleConfig->EngineBrakeEffect;

    if (VehicleConfig->TorqueCurve)
    {
        ChaosVehicleMovement->TorqueCurve.EditorCurveData = VehicleConfig->TorqueCurve->EditorCurveData;
    }

    // --- 变速箱 ---
    ChaosVehicleMovement->FinalRatio = VehicleConfig->FinalDriveRatio;
    ChaosVehicleMovement->bAutomaticTransmission = VehicleConfig->bAutomaticTransmission;
    ChaosVehicleMovement->GearAutoBoxLatency = VehicleConfig->GearAutoBoxLatency;
    ChaosVehicleMovement->UpShiftRPM = VehicleConfig->UpShiftRPM;
    ChaosVehicleMovement->DownShiftRPM = VehicleConfig->DownShiftRPM;

    // 齿轮比：将 FGearConfig 数组转换为 FVehicleTransmissionConfig 的 GearSetup
    ChaosVehicleMovement->TransmissionSetup.ForwardGears.Reset();
    ChaosVehicleMovement->TransmissionSetup.ReverseGears.Reset();

    for (int32 i = 0; i < VehicleConfig->GearRatios.Num(); ++i)
    {
        const FGearConfig& Gear = VehicleConfig->GearRatios[i];
        FVehicleGearData GearData;
        GearData.Ratio = Gear.Ratio;
        GearData.Efficiency = Gear.Efficiency;

        if (i == 0)
        {
            // 索引0 = 倒挡
            ChaosVehicleMovement->TransmissionSetup.ReverseGears.Add(GearData);
        }
        else if (i == 1)
        {
            // 索引1 = 空挡，Chaos 内部自动处理
            ChaosVehicleMovement->TransmissionSetup.NeutralGear = GearData;
        }
        else
        {
            // 索引2起 = 前进挡
            ChaosVehicleMovement->TransmissionSetup.ForwardGears.Add(GearData);
        }
    }

    // --- 转向 ---
    ChaosVehicleMovement->SteeringSetup.MaxSteerAngle = VehicleConfig->MaxSteeringAngle;

    // --- 悬挂 ---
    const FSuspensionConfig& Susp = VehicleConfig->SuspensionSetup;
    for (FWheelSetup& WheelSetup : ChaosVehicleMovement->WheelSetups)
    {
        WheelSetup.SuspensionMaxDrop = Susp.MaxDrop;
        WheelSetup.SuspensionMaxRaise = Susp.MaxRaise;
        WheelSetup.SuspensionSpringRate = Susp.SpringRate;
        WheelSetup.SuspensionDampingRate = Susp.DampingRate;
        WheelSetup.SuspensionPreLoad = Susp.PreLoad;
    }

    // --- 轮胎 ---
    const FTireConfig& Tire = VehicleConfig->TireSetup;
    for (FWheelSetup& WheelSetup : ChaosVehicleMovement->WheelSetups)
    {
        WheelSetup.TireConfig.LateralStiffness = Tire.LateralStiffness;
        WheelSetup.TireConfig.LongitudinalStiffness = Tire.LongitudinalStiffness;
        WheelSetup.TireConfig.FrictionScale = Tire.FrictionScale;
        WheelSetup.TireConfig.LatStiffnessLoad = Tire.LateralSlipFrictionScale;
    }

    // --- 制动 ---
    ChaosVehicleMovement->MaxBrakeTorque = VehicleConfig->MaxBrakeTorque;
    ChaosVehicleMovement->HandbrakeTorque = VehicleConfig->HandbrakeTorque;
}

// ============================================================================
// 初始化相机默认值（从 VehicleConfig 读取）
// ============================================================================
void ABaseVehiclePawn::InitCameraDefaults()
{
    if (!VehicleConfig) return;

    SpringArm->TargetArmLength = VehicleConfig->BaseArmLength;
    FollowCamera->FieldOfView = VehicleConfig->BaseFOV;
}

// ============================================================================
// 输入回调
// ============================================================================
void ABaseVehiclePawn::InputThrottle(const FInputActionValue& Value)
{
    if (!ChaosVehicleMovement) return;
    const float ThrottleInput = Value.Get<float>();
    ChaosVehicleMovement->SetThrottleInput(ThrottleInput);
}

void ABaseVehiclePawn::InputBrake(const FInputActionValue& Value)
{
    if (!ChaosVehicleMovement) return;
    const float BrakeInput = Value.Get<float>();
    ChaosVehicleMovement->SetBrakeInput(BrakeInput);
}

void ABaseVehiclePawn::InputSteering(const FInputActionValue& Value)
{
    if (!ChaosVehicleMovement) return;
    const float SteerInput = Value.Get<float>();
    ChaosVehicleMovement->SetSteeringInput(SteerInput);
}

void ABaseVehiclePawn::InputHandbrake(const FInputActionValue& Value)
{
    if (!ChaosVehicleMovement) return;
    const bool bHandbrake = Value.Get<bool>();
    ChaosVehicleMovement->SetHandbrakeInput(bHandbrake);
}

void ABaseVehiclePawn::InputExitVehicle(const FInputActionValue& Value)
{
    ExitVehicle();
}

// ============================================================================
// 动态相机：根据速度插值调整 FOV 和臂长
// ============================================================================
void ABaseVehiclePawn::UpdateDynamicCamera(float DeltaTime)
{
    if (!VehicleConfig || !SpringArm || !FollowCamera) return;

    // 获取速度-相机缩放曲线，若无则使用线性插值
    float TargetAlpha = 0.0f;

    if (VehicleConfig->SpeedCameraCurve)
    {
        // 曲线 X=速度(km/h)，Y=Alpha(0~1)
        TargetAlpha = FMath::Clamp(VehicleConfig->SpeedCameraCurve->GetFloatValue(FMath::Abs(CurrentSpeedKPH)), 0.0f, 1.0f);
    }
    else
    {
        // 无曲线时：以 200 km/h 为最大参考速度做线性映射
        TargetAlpha = FMath::Clamp(FMath::Abs(CurrentSpeedKPH) / 200.0f, 0.0f, 1.0f);
    }

    // 平滑插值
    CameraAlpha = FMath::FInterpTo(CameraAlpha, TargetAlpha, DeltaTime, 3.0f);

    // 插值臂长
    const float ArmLength = FMath::Lerp(VehicleConfig->BaseArmLength, VehicleConfig->MaxArmLength, CameraAlpha);
    SpringArm->TargetArmLength = ArmLength;

    // 插值 FOV
    const float FOV = FMath::Lerp(VehicleConfig->BaseFOV, VehicleConfig->MaxFOV, CameraAlpha);
    FollowCamera->FieldOfView = FOV;
}

// ============================================================================
// 进入载具
// ============================================================================
void ABaseVehiclePawn::EnterVehicle(ACharacter* InDriver)
{
    if (!InDriver || Driver) return;

    // 服务器端执行，客户端通过 RPC 或 RepNotify 同步
    Driver = InDriver;

    // 隐藏驾驶员 Mesh，挂载到载具骨骼 Socket
    USkeletalMeshComponent* DriverMesh = InDriver->GetMesh();
    if (DriverMesh)
    {
        DriverMesh->bHiddenInGame = true;
        DriverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DriverMesh->AttachToComponent(GetMesh(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            VehicleConfig ? VehicleConfig->DriverSocketName : FName("DriverSeat"));
    }

    // 关闭驾驶员碰撞
    InDriver->SetActorEnableCollision(false);
    if (UCharacterMovementComponent* CMC = InDriver->GetCharacterMovement())
    {
        CMC->StopMovementImmediately();
        CMC->SetMovementMode(MOVE_None);
    }

    // 切换控制器 Possess
    if (APlayerController* PC = InDriver->GetController<APlayerController>())
    {
        PC->Possess(this);
    }
}

// ============================================================================
// 离开载具
// ============================================================================
void ABaseVehiclePawn::ExitVehicle()
{
    if (!Driver) return;

    ACharacter* PreviousDriver = Driver;

    // 查找安全下车位置
    FVector ExitLocation;
    if (!FindSafeExitLocation(ExitLocation))
    {
        // 找不到安全位置，不允许下车
        return;
    }

    // 恢复驾驶员 Mesh
    USkeletalMeshComponent* DriverMesh = PreviousDriver->GetMesh();
    if (DriverMesh)
    {
        DriverMesh->bHiddenInGame = false;
        DriverMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        DriverMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }

    // 恢复驾驶员碰撞和移动
    PreviousDriver->SetActorEnableCollision(true);
    if (UCharacterMovementComponent* CMC = PreviousDriver->GetCharacterMovement())
    {
        CMC->SetMovementMode(MOVE_Walking);
    }

    // 设置下车位置
    PreviousDriver->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // 切换控制器 Possess 回驾驶员
    if (APlayerController* PC = GetController<APlayerController>())
    {
        PC->Possess(PreviousDriver);
    }

    // 清除驾驶员引用
    Driver = nullptr;

    // 重置载具输入
    if (ChaosVehicleMovement)
    {
        ChaosVehicleMovement->SetThrottleInput(0.0f);
        ChaosVehicleMovement->SetSteeringInput(0.0f);
        ChaosVehicleMovement->SetBrakeInput(0.0f);
        ChaosVehicleMovement->SetHandbrakeInput(false);
    }

    // 移除载具输入映射上下文
    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->RemoveMappingContext(VehicleIMC);
                }
            }
        }
    }
}

// ============================================================================
// 查找安全下车位置
// ============================================================================
bool ABaseVehiclePawn::FindSafeExitLocation(FVector& OutLocation) const
{
    const FVector BaseOffset = VehicleConfig ? VehicleConfig->ExitOffset : FVector(-200.0f, 150.0f, 0.0f);
    const float CheckRadius = VehicleConfig ? VehicleConfig->ExitCheckRadius : 50.0f;

    // 将偏移从载具本地坐标系转换到世界坐标系
    const FVector CandidateLocation = GetActorTransform().TransformPositionNoScale(BaseOffset);

    // 碰撞检测：检查候选位置是否有障碍物
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Driver);

    const FCollisionShape CheckShape = FCollisionShape::MakeSphere(CheckRadius);

    // 优先检查左侧
    bool bLeftClear = !GetWorld()->OverlapAnyTestByChannel(
        CandidateLocation, FQuat::Identity, ECC_Pawn, CheckShape, QueryParams);

    if (bLeftClear)
    {
        OutLocation = CandidateLocation;
        return true;
    }

    // 左侧被挡，尝试右侧（Y 取反）
    const FVector RightOffset(BaseOffset.X, -BaseOffset.Y, BaseOffset.Z);
    const FVector RightLocation = GetActorTransform().TransformPositionNoScale(RightOffset);

    bool bRightClear = !GetWorld()->OverlapAnyTestByChannel(
        RightLocation, FQuat::Identity, ECC_Pawn, CheckShape, QueryParams);

    if (bRightClear)
    {
        OutLocation = RightLocation;
        return true;
    }

    // 两侧都不安全
    return false;
}

// ============================================================================
// 网络同步回调：驾驶员变化
// ============================================================================
void ABaseVehiclePawn::OnRep_Driver()
{
    // 客户端收到 Driver 同步后，可在此处理 UI 更新等
    // 目前留空，后续可扩展
}
