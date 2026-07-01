// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Vehicles/WheeledVehiclePawnBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AOpenWorldPlayerController::AOpenWorldPlayerController()
{
}

void AOpenWorldPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // TODO: 大世界专属初始化（如加载大地图 IMC、滑翔伞输入等）
}

void AOpenWorldPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 钩索（大世界专属）
        if (IA_Hook) EnhancedInputComponent->BindAction(IA_Hook, ETriggerEvent::Started, this, &AOpenWorldPlayerController::Input_Hook);
    }
}

// --- 大世界专属输入回调 ---

void AOpenWorldPlayerController::Input_Hook()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (HookStartEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, HookStartEventTag, FGameplayEventData());
    }
}

// ============================================================================
// 载具驾驶：Client RPC - 预混合准备（拦截 Possess 自动相机管理）
// ============================================================================
void AOpenWorldPlayerController::Client_PrepareForCameraBlend_Implementation()
{
    // 【核心修复】关闭 Possess 时的自动镜头跳转，保护平滑过渡逻辑
    bAutoManageActiveCameraTarget = false;
}

// ============================================================================
// 载具驾驶：Server RPC - 上车
// ============================================================================
bool AOpenWorldPlayerController::Server_PossessVehicle_Validate(AWheeledVehiclePawnBase* TargetVehicle)
{
    return TargetVehicle != nullptr;
}

void AOpenWorldPlayerController::Server_PossessVehicle_Implementation(AWheeledVehiclePawnBase* TargetVehicle)
{
    if (!HasAuthority() || !TargetVehicle) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
    if (!PlayerChar || TargetVehicle->Driver != nullptr) return;

    // 1. 角色执行上车准备：关闭移动、切换碰撞通道、Attach 到驾驶座 Socket
    PlayerChar->PrepareForDriving(TargetVehicle, TargetVehicle->GetDriverSeatSocketName());

    // 2. 设置载具的驾驶员引用
    TargetVehicle->Driver = PlayerChar;

    // 3. UnPossess 角色，Possess 载具
    Client_PrepareForCameraBlend();
    UnPossess();
    Possess(TargetVehicle);

    // 4. 客户端：平滑过渡相机
    Client_BlendCameraToVehicle(TargetVehicle);
}

// ============================================================================
// 载具驾驶：Server RPC - 下车
// ============================================================================
bool AOpenWorldPlayerController::Server_UnPossessVehicle_Validate(FVector ExitLocation)
{
    return true;
}

void AOpenWorldPlayerController::Server_UnPossessVehicle_Implementation(FVector ExitLocation)
{
    if (!HasAuthority()) return;

    AWheeledVehiclePawnBase* Vehicle = Cast<AWheeledVehiclePawnBase>(GetPawn());
    if (!Vehicle) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Vehicle->Driver);
    if (!PlayerChar) return;

    FGameplayTag UnmountTag = Vehicle->GetUnmountVehicleEventTag();

    Vehicle->Driver = nullptr;
    Vehicle->ResetVehicleInputs();

    Client_PrepareForCameraBlend();
    UnPossess();
    Possess(PlayerChar);

    PlayerChar->EndDriving(ExitLocation);

    Client_BlendCameraToCharacter(PlayerChar, Vehicle);

    if (UnmountTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerChar, UnmountTag, FGameplayEventData());
    }
}

// ============================================================================
// 载具驾驶：Client RPC - 相机过渡到载具
// ============================================================================
void AOpenWorldPlayerController::Client_BlendCameraToVehicle_Implementation(AWheeledVehiclePawnBase* TargetVehicle)
{
    if (!TargetVehicle) return;

    // 镜头从人平滑拉升到车
    SetViewTargetWithBlend(TargetVehicle, VehicleCameraBlendTime, VTBlend_Cubic);

    // 过渡结束后恢复自动管理
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        bAutoManageActiveCameraTarget = true;
    }, VehicleCameraBlendTime, false);
}

// ============================================================================
// 载具驾驶：Client RPC - 相机过渡回角色，清理载具 IMC
// ============================================================================
void AOpenWorldPlayerController::Client_BlendCameraToCharacter_Implementation(APlayerCharacter* InCharacter, AWheeledVehiclePawnBase* OldVehicle)
{
    if (!InCharacter) return;

    SetViewTargetWithBlend(InCharacter, CharacterCameraBlendTime, VTBlend_Cubic);

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        bAutoManageActiveCameraTarget = true;
    }, CharacterCameraBlendTime, false);

    if (OldVehicle && OldVehicle->VehicleIMC)
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Subsystem->RemoveMappingContext(OldVehicle->VehicleIMC);
            }
        }
    }
}
