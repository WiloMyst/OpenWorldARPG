// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_MountVehicleBase.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Components/WeaponManagerComponent.h"
#include "Vehicles/WheeledVehiclePawnBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/Tasks/AbilityTask_NavMoveTo.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Kismet/KismetMathLibrary.h"

UGA_MountVehicleBase::UGA_MountVehicleBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    bHasExecutedMount = false;
}

// ============================================================================
// 阶段 1：ActivateAbility — 解析载具，启动 NavMesh 寻路
// ============================================================================
void UGA_MountVehicleBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    bHasExecutedMount = false;

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!PlayerChar || !TriggerEventData)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 从 EventData 解析目标载具（OptionalObject 在 const EventData 中为 const，需去常量转换）
    TargetVehicle = Cast<AWheeledVehiclePawnBase>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
    if (!TargetVehicle)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // --- 阶段 1：NavMesh 寻路 (Approach) ---

    // 获取车门交互位置
    const FTransform InteractionTransform = TargetVehicle->GetInteractionTargetTransform_Implementation();
    const FVector DoorLocation = InteractionTransform.GetLocation();

    // 如果角色已经在车门附近，直接跳过寻路，进入转身绑定阶段，防止 NavMesh 寻路原地报错卡死
    const float DistToDoor2D = FVector::Dist2D(PlayerChar->GetActorLocation(), DoorLocation);
    if (DistToDoor2D <= ApproachAcceptanceRadius)
    {
        OnApproachReached();
        return; // 直接返回，后续逻辑在 OnApproachReached 中继续
    }

    // 使用自定义 NavMoveTo Task 沿 NavMesh 寻路避障走向车门
    NavMoveToTask = UAbilityTask_NavMoveTo::CreateNavMoveToTask(
        this, DoorLocation, ApproachAcceptanceRadius);

    if (NavMoveToTask)
    {
        // 串联回调：到达 → OnApproachReached（阶段 2），失败 → OnApproachFailed（取消）
        NavMoveToTask->OnTargetReached.AddDynamic(this, &UGA_MountVehicleBase::OnApproachReached);
        NavMoveToTask->OnFailed.AddDynamic(this, &UGA_MountVehicleBase::OnApproachFailed);
        NavMoveToTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

// ============================================================================
// 阶段 1 回调：寻路失败
// ============================================================================
void UGA_MountVehicleBase::OnApproachFailed()
{
    // 寻路失败（无路径 / 被阻挡 / 超时），取消上车
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// ============================================================================
// 阶段 2：OnApproachReached — 转身对齐 + 物理绑定 + Motion Warping + 播放蒙太奇
// ============================================================================
void UGA_MountVehicleBase::OnApproachReached()
{
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CurrentActorInfo->AvatarActor.Get());
    if (!PlayerChar || !TargetVehicle)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // --- 2a：转身对齐 — 角色朝向车辆 ---
    const FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(
        PlayerChar->GetActorLocation(), TargetVehicle->GetActorLocation());
    PlayerChar->SetActorRotation(FRotator(0.0f, TargetRot.Yaw, 0.0f));

    // --- 2b：物理绑定 (高内聚重构) ---
    // 直接调用 Character 内部封装好的准备函数，GAS 客户端预测会自动在本地和 Server 同步执行
    PlayerChar->PrepareForDriving(TargetVehicle, TargetVehicle->GetDriverSeatSocketName());

    // --- 2c：Motion Warping — 动态跟随车门的 Component Warp Target ---
    if (UMotionWarpingComponent* MotionWarping = PlayerChar->GetMotionWarpingComp())
    {
        // 关键：使用 AddOrUpdateWarpTargetFromComponent + bFollowComponent=true
        // 使 Warp 目标随载具 Mesh 的 InteractionSocket 实时更新，
        // 角色在动画期间与车门保持相对静止，即使车辆在移动也不会脱节
        MotionWarping->AddOrUpdateWarpTargetFromComponent(
            WarpTargetName,
            TargetVehicle->GetMesh(),
            TargetVehicle->GetInteractionSocketName(),
            true // bFollowComponent
        );
    }

    // --- 2d：播放上车蒙太奇（阶段 3 的前置） ---
    if (MountMontage)
    {
        PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, MountMontage, 1.0f, NAME_None, false);

        if (PlayMontageTask)
        {
            PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_MountVehicleBase::OnMontageCompleted);
            PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_MountVehicleBase::OnMontageBlendOut);
            PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_MountVehicleBase::OnMontageInterrupted);
            PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_MountVehicleBase::OnMontageCancelled);
            PlayMontageTask->ReadyForActivation();
        }
        else
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
    }
    else
    {
        // 无蒙太奇配置，直接执行上车
        ExecuteMount();
    }
}

// ============================================================================
// 阶段 3：ExecuteMount — 交接控制权
// ============================================================================
void UGA_MountVehicleBase::OnMontageCompleted()
{
    ExecuteMount();
}

void UGA_MountVehicleBase::OnMontageBlendOut()
{
    ExecuteMount();
}

void UGA_MountVehicleBase::ExecuteMount()
{
    if (bHasExecutedMount) return;
    bHasExecutedMount = true;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CurrentActorInfo->AvatarActor.Get());
    if (PlayerChar && TargetVehicle)
    {
        PlayerChar->SetActorHiddenInGame(true);
        if (UWeaponManagerComponent* WeaponMgr = PlayerChar->GetWeaponManagerComponent_Implementation())
        {
            WeaponMgr->SetWeaponHidden(true);
        }

        if (AOpenWorldPlayerController* OW_PC = Cast<AOpenWorldPlayerController>(PlayerChar->GetController()))
        {
            OW_PC->Server_PossessVehicle(TargetVehicle);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GA_MountVehicleBase 失败：当前 Controller 不是 AOpenWorldPlayerController！请检查 GameMode 设置。"));
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MountVehicleBase::OnMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_MountVehicleBase::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_MountVehicleBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (NavMoveToTask)
    {
        NavMoveToTask->EndTask();
        NavMoveToTask = nullptr;
    }

    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }

    // 清理 Motion Warping 目标，防止残留
    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get()))
    {
        if (UMotionWarpingComponent* MotionWarping = PlayerChar->GetMotionWarpingComp())
        {
            MotionWarping->RemoveWarpTarget(WarpTargetName);
        }
    }

    TargetVehicle = nullptr;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
