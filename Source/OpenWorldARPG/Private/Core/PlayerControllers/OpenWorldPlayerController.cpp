// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
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
bool AOpenWorldPlayerController::Server_PossessVehicle_Validate(AVehiclePawnBase* TargetVehicle)
{
    if (!TargetVehicle) return false;

    // 防瞬移上车作弊：玩家与载具距离不得超过 MaxEnterVehicleDistance
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
    if (!PlayerChar) return false;

    const float Dist = FVector::Dist(PlayerChar->GetActorLocation(), TargetVehicle->GetActorLocation());
    if (Dist > MaxEnterVehicleDistance) return false;

    // 载具不能已被其他玩家驾驶
    if (TargetVehicle->GetDriver() != nullptr) return false;

    return true;
}

void AOpenWorldPlayerController::Server_PossessVehicle_Implementation(AVehiclePawnBase* TargetVehicle)
{
    if (!HasAuthority() || !TargetVehicle) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
    if (!PlayerChar || TargetVehicle->GetDriver() != nullptr) return;

    // GA_MountVehicleBase 已在播放动画前调用过 PrepareForDriving，此处不再重复

    TargetVehicle->SetDriver(PlayerChar);

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
    // 必须正在驾驶载具才能下车
    AVehiclePawnBase* Vehicle = Cast<AVehiclePawnBase>(GetPawn());
    if (!Vehicle) return false;

    // 防劫持：当前载具必须有驾驶员（即本 Controller 之前 Possess 的角色）
    if (Vehicle->GetDriver() == nullptr) return false;

    return true;
}

void AOpenWorldPlayerController::Server_UnPossessVehicle_Implementation(FVector ExitLocation)
{
    if (!HasAuthority()) return;

    AVehiclePawnBase* Vehicle = Cast<AVehiclePawnBase>(GetPawn());
    if (!Vehicle) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Vehicle->GetDriver());
    if (!PlayerChar) return;

    FGameplayTag UnmountTag = Vehicle->GetUnmountVehicleEventTag();

    // 1. 清除载具驾驶员引用并重置输入
    Vehicle->SetDriver(nullptr);
    Vehicle->ResetVehicleInputs();

    // 2. 交接控制权：从载具切回角色（角色仍然 Attach 在车座上，不执行 EndDriving）
    //    物理脱离（Detach / 恢复碰撞 / 恢复移动）由 GA_UnmountVehicleBase 在下车动画结束后延迟执行
    Client_PrepareForCameraBlend();
    UnPossess();
    Possess(PlayerChar);

    // 3. 视角平滑切回角色
    Client_BlendCameraToCharacter(PlayerChar, Vehicle);

    // 4. 发送下车事件，激活 GA_UnmountVehicleBase（角色在载具局部坐标系中播放下车蒙太奇）
    if (UnmountTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerChar, UnmountTag, FGameplayEventData());
    }
}

// ============================================================================
// 载具驾驶：Client RPC - 相机过渡到载具
// ============================================================================
void AOpenWorldPlayerController::Client_BlendCameraToVehicle_Implementation(AVehiclePawnBase* TargetVehicle)
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
void AOpenWorldPlayerController::Client_BlendCameraToCharacter_Implementation(APlayerCharacter* InCharacter, AVehiclePawnBase* OldVehicle)
{
    if (!InCharacter) return;

    SetViewTargetWithBlend(InCharacter, CharacterCameraBlendTime, VTBlend_Cubic);

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        bAutoManageActiveCameraTarget = true;
    }, CharacterCameraBlendTime, false);

    if (OldVehicle)
    {
        // 在客户端本地显式清空载具输入，防止 InputComponent 剥离导致 Completed 事件丢失 (Sticky Input)
        OldVehicle->ResetVehicleInputs();

        if (OldVehicle->GetVehicleIMC())
        {
            if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->RemoveMappingContext(OldVehicle->GetVehicleIMC());
                }
            }
        }
    }
}
