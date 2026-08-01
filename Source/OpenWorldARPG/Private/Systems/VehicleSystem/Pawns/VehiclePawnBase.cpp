// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "Systems/VehicleSystem/Components/VehicleMovementComponent_Predictive.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Net/UnrealNetwork.h"

AVehiclePawnBase::AVehiclePawnBase()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
}

// --- 生命周期 ---

void AVehiclePawnBase::BeginPlay()
{
    Super::BeginPlay();

    if (FollowCamera)
    {
        BaseFOV = FollowCamera->FieldOfView;
    }
}

void AVehiclePawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 统一注册载具 IMC（Possess 时必定调用，比 PawnClientRestart 更可靠）
    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->AddMappingContext(VehicleIMC, 1);
            }
        }
    }

    // 统一绑定下车输入
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_ExitVehicle)
        {
            EIC->BindAction(IA_ExitVehicle, ETriggerEvent::Started, this, &AVehiclePawnBase::InputExitVehicle);
        }
    }
}

void AVehiclePawnBase::PawnClientRestart()
{
    Super::PawnClientRestart();

    // 备份路径：某些 possess 时序下 SetupPlayerInputComponent 可能晚于首次输入
    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->AddMappingContext(VehicleIMC, 1);
            }
        }
    }
}

void AVehiclePawnBase::UnPossessed()
{
    Super::UnPossessed();

    ResetVehicleInputs();

    if (VehicleIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->RemoveMappingContext(VehicleIMC);
            }
        }
    }
}

void AVehiclePawnBase::FellOutOfWorld(const UDamageType& dmgType)
{
    // 默认空实现：飞行/悬浮/水面载具不掉出世界。
    // 轮式载具 override 做重生处理。
}

// --- 统一下车输入回调 ---

void AVehiclePawnBase::InputExitVehicle(const FInputActionValue& Value)
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
}

// --- IInteractableInterface 统一实现 ---

bool AVehiclePawnBase::CanInteract_Implementation(ACharacter* InstigatorCharacter) const
{
    return Driver == nullptr && InstigatorCharacter != nullptr;
}

void AVehiclePawnBase::OnInteract_Implementation(ACharacter* InstigatorCharacter)
{
    if (!InstigatorCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] OnInteract: InstigatorCharacter 为 null"));
        return;
    }
    if (!MountVehicleEventTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] OnInteract: MountVehicleEventTag 未配置! 请在载具蓝图设置此 Tag"));
        return;
    }

    // 统一走 GAS 事件流程：发送 MountVehicleEventTag 给角色，
    // 由 GA_MountVehicleBase 处理寻路/转身/动画/Possess 全流程
    FGameplayEventData EventData;
    EventData.Instigator = InstigatorCharacter;
    EventData.Target = this;
    EventData.OptionalObject = this;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        InstigatorCharacter, MountVehicleEventTag, EventData);
}

FTransform AVehiclePawnBase::GetInteractionTargetTransform_Implementation() const
{
    if (USkeletalMeshComponent* Mesh = GetVehicleMesh())
    {
        if (Mesh->DoesSocketExist(InteractionSocketName))
        {
            return Mesh->GetSocketTransform(InteractionSocketName);
        }
    }

    FTransform FallbackTransform = GetActorTransform();
    FallbackTransform.AddToTranslation(GetActorForwardVector() * 200.0f);
    return FallbackTransform;
}

// --- 网络复制回调 ---

void AVehiclePawnBase::OnRep_ReplicatedMovement()
{
    // 本地控制端 + 预测移动组件：交给移动组件做预测纠错，跳过默认 snap
    if (IsLocallyControlled() && !HasAuthority())
    {
        if (UVehicleMovementComponent_Predictive* PredictiveMoveComp =
            Cast<UVehicleMovementComponent_Predictive>(GetMovementComponent()))
        {
            PredictiveMoveComp->OnServerStateReceived(GetReplicatedMovement());
            return;
        }
    }

    // 远端客户端 / Chaos 载具：默认行为（snap + 引擎平滑）
    Super::OnRep_ReplicatedMovement();
}

// --- 网络复制 ---

void AVehiclePawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVehiclePawnBase, Driver);
}
