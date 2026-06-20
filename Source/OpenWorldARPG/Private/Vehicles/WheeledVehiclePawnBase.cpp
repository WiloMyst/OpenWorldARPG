// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Vehicles/WheeledVehiclePawnBase.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
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
AWheeledVehiclePawnBase::AWheeledVehiclePawnBase()
{
    // 网络同步
    bReplicates = true;
    SetReplicatingMovement(true);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    // 注意：AWheeledVehiclePawn 已在父类构造函数中创建了
    // VehicleMovementComponent (UChaosWheeledVehicleMovementComponent) 和 Mesh，
    // 这里不需要再次 CreateDefaultSubobject

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
// BeginPlay：缓存移动组件指针，从 DataAsset 读取配置并应用
// ============================================================================
void AWheeledVehiclePawnBase::BeginPlay()
{
    Super::BeginPlay();

    // 缓存 Wheeled 移动组件指针（父类以基类指针存储，这里做一次 Cast）
    CachedWheeledMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

    // 在物理车辆创建之前应用配置，确保参数生效
    ApplyVehicleConfig();
    InitCameraDefaults();
}

// ============================================================================
// Tick：更新动态相机
// ============================================================================
void AWheeledVehiclePawnBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 缓存当前速度 (km/h)
    if (CachedWheeledMovement)
    {
        // GetForwardSpeed() 返回 cm/s，转换为 km/h
        CurrentSpeedKPH = CachedWheeledMovement->GetForwardSpeed() * 0.036f;
    }

    UpdateDynamicCamera(DeltaTime);
}

// ============================================================================
// 输入绑定：Enhanced Input
// ============================================================================
void AWheeledVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 油门：1D 轴（W/S 或扳机）
        if (IA_Throttle)
        {
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputThrottle);
            EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputThrottle);
        }
        // 刹车/倒车
        if (IA_Brake)
        {
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputBrake);
            EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputBrake);
        }
        // 转向：1D 轴（A/D 或左摇杆）
        if (IA_Steering)
        {
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputSteering);
            EnhancedInput->BindAction(IA_Steering, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputSteering);
        }
        // 手刹
        if (IA_Handbrake)
        {
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Triggered, this, &AWheeledVehiclePawnBase::InputHandbrake);
            EnhancedInput->BindAction(IA_Handbrake, ETriggerEvent::Completed, this, &AWheeledVehiclePawnBase::InputHandbrake);
        }
        // 下车
        if (IA_ExitVehicle)
        {
            EnhancedInput->BindAction(IA_ExitVehicle, ETriggerEvent::Started, this, &AWheeledVehiclePawnBase::InputExitVehicle);
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
void AWheeledVehiclePawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWheeledVehiclePawnBase, Driver);
}

// ============================================================================
// 从 VehicleConfig 读取参数并应用到移动组件和车轮实例
// UE5.5 Chaos Vehicles 使用 FVehicleEngineConfig / FVehicleTransmissionConfig /
// FVehicleSteeringConfig 等结构体，悬挂和轮胎参数在 UChaosVehicleWheel 上
// ============================================================================
void AWheeledVehiclePawnBase::ApplyVehicleConfig()
{
    if (!VehicleConfig || !CachedWheeledMovement) return;

    // --- 车身物理 ---
    CachedWheeledMovement->Mass = VehicleConfig->Mass;
    CachedWheeledMovement->DragCoefficient = VehicleConfig->DragCoefficient;
    CachedWheeledMovement->DownforceCoefficient = VehicleConfig->DownforceCoefficient;

    // --- 引擎配置 (FVehicleEngineConfig) ---
    FVehicleEngineConfig& Engine = CachedWheeledMovement->EngineSetup;
    Engine.MaxRPM = VehicleConfig->MaxRPM;
    Engine.EngineIdleRPM = VehicleConfig->IdleRPM;
    Engine.EngineBrakeEffect = VehicleConfig->EngineBrakeEffect;

    // 转矩曲线：将 UCurveFloat 的 FRichCurve 拷贝到 FRuntimeFloatCurve
    if (VehicleConfig->TorqueCurve)
    {
        Engine.TorqueCurve.EditorCurveData = VehicleConfig->TorqueCurve->FloatCurve;
    }

    // --- 变速箱配置 (FVehicleTransmissionConfig) ---
    FVehicleTransmissionConfig& Transmission = CachedWheeledMovement->TransmissionSetup;
    Transmission.bUseAutomaticGears = VehicleConfig->bAutomaticTransmission;
    Transmission.bUseAutoReverse = VehicleConfig->bAutomaticTransmission;
    Transmission.FinalRatio = VehicleConfig->FinalDriveRatio;
    Transmission.ChangeUpRPM = VehicleConfig->UpShiftRPM;
    Transmission.ChangeDownRPM = VehicleConfig->DownShiftRPM;
    Transmission.GearChangeTime = VehicleConfig->GearAutoBoxLatency;

    // 将 FGearConfig 数组映射到 UE5.5 的 ForwardGearRatios / ReverseGearRatios
    Transmission.ForwardGearRatios.Reset();
    Transmission.ReverseGearRatios.Reset();

    for (int32 i = 0; i < VehicleConfig->GearRatios.Num(); ++i)
    {
        const FGearConfig& Gear = VehicleConfig->GearRatios[i];
        if (i == 0)
        {
            // 索引0 = 倒挡
            Transmission.ReverseGearRatios.Add(Gear.Ratio);
            Transmission.TransmissionEfficiency = Gear.Efficiency;
        }
        else if (i >= 2)
        {
            // 索引2起 = 前进挡（索引1 = 空挡，Chaos 内部自动处理）
            Transmission.ForwardGearRatios.Add(Gear.Ratio);
        }
    }

    // --- 转向配置 (FVehicleSteeringConfig) ---
    FVehicleSteeringConfig& Steering = CachedWheeledMovement->SteeringSetup;
    if (VehicleConfig->SpeedSteeringCurve)
    {
        Steering.SteeringCurve.EditorCurveData = VehicleConfig->SpeedSteeringCurve->FloatCurve;
    }

    // --- 车轮实例：悬挂 + 轮胎 + 制动 ---
    // UE5.5 中这些参数在 UChaosVehicleWheel 实例上设置
    const FSuspensionConfig& Susp = VehicleConfig->SuspensionSetup;
    const FTireConfig& Tire = VehicleConfig->TireSetup;

    for (UChaosVehicleWheel* Wheel : CachedWheeledMovement->Wheels)
    {
        if (!Wheel) continue;

        // 悬挂参数
        Wheel->SpringRate = Susp.SpringRate;
        Wheel->SuspensionDampingRatio = FMath::Clamp(Susp.DampingRate / 10000.0f, 0.0f, 1.0f);
        Wheel->SuspensionMaxDrop = Susp.MaxDrop;
        Wheel->SuspensionMaxRaise = Susp.MaxRaise;
        Wheel->SpringPreload = Susp.PreLoad;

        // 轮胎参数
        Wheel->CorneringStiffness = Tire.LateralStiffness;
        Wheel->FrictionForceMultiplier = Tire.FrictionScale;
        Wheel->SideSlipModifier = Tire.LateralSlipFrictionScale;

        // 制动参数
        Wheel->MaxBrakeTorque = VehicleConfig->MaxBrakeTorque;
        Wheel->MaxHandBrakeTorque = VehicleConfig->HandbrakeTorque;

        // 转向角度（仅对受转向影响的车轮生效）
        if (Wheel->bAffectedBySteering)
        {
            Wheel->MaxSteerAngle = VehicleConfig->MaxSteeringAngle;
        }
    }
}

// ============================================================================
// 初始化相机默认值（从 VehicleConfig 读取）
// ============================================================================
void AWheeledVehiclePawnBase::InitCameraDefaults()
{
    if (!VehicleConfig) return;

    SpringArm->TargetArmLength = VehicleConfig->BaseArmLength;
    FollowCamera->FieldOfView = VehicleConfig->BaseFOV;
}

// ============================================================================
// 输入回调
// ============================================================================
void AWheeledVehiclePawnBase::InputThrottle(const FInputActionValue& Value)
{
    if (!CachedWheeledMovement) return;
    CachedWheeledMovement->SetThrottleInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputBrake(const FInputActionValue& Value)
{
    if (!CachedWheeledMovement) return;
    CachedWheeledMovement->SetBrakeInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputSteering(const FInputActionValue& Value)
{
    if (!CachedWheeledMovement) return;
    CachedWheeledMovement->SetSteeringInput(Value.Get<float>());
}

void AWheeledVehiclePawnBase::InputHandbrake(const FInputActionValue& Value)
{
    if (!CachedWheeledMovement) return;
    CachedWheeledMovement->SetHandbrakeInput(Value.Get<bool>());
}

void AWheeledVehiclePawnBase::InputExitVehicle(const FInputActionValue& Value)
{
    ExitVehicle();
}

// ============================================================================
// 动态相机：根据速度插值调整 FOV 和臂长
// ============================================================================
void AWheeledVehiclePawnBase::UpdateDynamicCamera(float DeltaTime)
{
    if (!VehicleConfig || !SpringArm || !FollowCamera) return;

    // 获取速度-相机缩放曲线，若无则使用线性插值
    float TargetAlpha = 0.0f;

    if (VehicleConfig->SpeedCameraCurve)
    {
        // 曲线 X=速度(km/h)，Y=Alpha(0~1)
        TargetAlpha = FMath::Clamp(
            VehicleConfig->SpeedCameraCurve->GetFloatValue(FMath::Abs(CurrentSpeedKPH)),
            0.0f, 1.0f);
    }
    else
    {
        // 无曲线时：以 200 km/h 为最大参考速度做线性映射
        TargetAlpha = FMath::Clamp(FMath::Abs(CurrentSpeedKPH) / 200.0f, 0.0f, 1.0f);
    }

    // 平滑插值
    CameraAlpha = FMath::FInterpTo(CameraAlpha, TargetAlpha, DeltaTime, 3.0f);

    // 插值臂长
    SpringArm->TargetArmLength = FMath::Lerp(VehicleConfig->BaseArmLength, VehicleConfig->MaxArmLength, CameraAlpha);

    // 插值 FOV
    FollowCamera->FieldOfView = FMath::Lerp(VehicleConfig->BaseFOV, VehicleConfig->MaxFOV, CameraAlpha);
}

// ============================================================================
// 进入载具
// ============================================================================
void AWheeledVehiclePawnBase::EnterVehicle(ACharacter* InDriver)
{
    if (!InDriver || Driver) return;

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

    // 关闭驾驶员碰撞和移动
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
void AWheeledVehiclePawnBase::ExitVehicle()
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
    if (CachedWheeledMovement)
    {
        CachedWheeledMovement->SetThrottleInput(0.0f);
        CachedWheeledMovement->SetSteeringInput(0.0f);
        CachedWheeledMovement->SetBrakeInput(0.0f);
        CachedWheeledMovement->SetHandbrakeInput(false);
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
bool AWheeledVehiclePawnBase::FindSafeExitLocation(FVector& OutLocation) const
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
void AWheeledVehiclePawnBase::OnRep_Driver()
{
    // 客户端收到 Driver 同步后，可在此处理 UI 更新等
}
