// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_UnmountVehicleBase.h"
#include "Characters/PlayerCharacter.h"
#include "Components/WeaponManagerComponent.h"
#include "Vehicles/WheeledVehiclePawnBase.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_UnmountVehicleBase::UGA_UnmountVehicleBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    bHasUnbound = false;
}

void UGA_UnmountVehicleBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    bHasUnbound = false;

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!PlayerChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    PlayerChar->SetActorHiddenInGame(false);
    if (UWeaponManagerComponent* WeaponMgr = PlayerChar->GetWeaponManagerComponent_Implementation())
    {
        WeaponMgr->SetWeaponHidden(false);
    }

    // 从角色的 Attachment Parent 获取载具引用
    // （Server_UnPossessVehicle 已交接控制权，但角色仍然 Attach 在车座上）
    TargetVehicle = Cast<AWheeledVehiclePawnBase>(PlayerChar->GetAttachParentActor());

    // 播放下车蒙太奇 — 角色在载具局部坐标系中播放，天然保持相对静止
    if (UnmountMontage)
    {
        PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, UnmountMontage, 1.0f, NAME_None, false);

        if (PlayMontageTask)
        {
            PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_UnmountVehicleBase::OnMontageCompleted);
            PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_UnmountVehicleBase::OnMontageBlendOut);
            PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_UnmountVehicleBase::OnMontageInterrupted);
            PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_UnmountVehicleBase::OnMontageCancelled);
            PlayMontageTask->ReadyForActivation();
        }
        else
        {
            // Task 创建失败，直接执行物理脱离兜底
            ExecutePhysicalUnbind();
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        // 无蒙太奇配置，直接执行物理脱离
        ExecutePhysicalUnbind();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

// ============================================================================
// 物理脱离：在动画结束后才执行 Detach + 恢复碰撞 + 恢复移动
// ============================================================================
void UGA_UnmountVehicleBase::ExecutePhysicalUnbind()
{
    if (bHasUnbound) return;
    bHasUnbound = true;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CurrentActorInfo->AvatarActor.Get());
    if (!PlayerChar) return;

    FVector FinalExitLocation = PlayerChar->GetActorLocation(); // 默认兜底

    // 强制向载具索要左前门的绝对安全位置
    if (TargetVehicle)
    {
        FVector SafeLocation;
        if (TargetVehicle->FindSafeExitLocation(SafeLocation))
        {
            // 拿到车门位置后，强制把 Z 轴拔高一个胶囊体半高，确保双脚落地不穿模
            const float CapsuleHalfHeight = PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            SafeLocation.Z += CapsuleHalfHeight;
            FinalExitLocation = SafeLocation;
        }
    }

    // 执行真正的物理脱离：DetachFromActor + 恢复碰撞 + 恢复移动模式
    PlayerChar->EndDriving(FinalExitLocation);
}

// ============================================================================
// 蒙太奇回调
// ============================================================================
void UGA_UnmountVehicleBase::OnMontageCompleted()
{
    ExecutePhysicalUnbind();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_UnmountVehicleBase::OnMontageBlendOut()
{
    ExecutePhysicalUnbind();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_UnmountVehicleBase::OnMontageInterrupted()
{
    // 被打断时也需要执行物理脱离，否则角色会永远卡在载具上
    ExecutePhysicalUnbind();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_UnmountVehicleBase::OnMontageCancelled()
{
    ExecutePhysicalUnbind();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_UnmountVehicleBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }

    // 兜底：如果 EndAbility 时仍未执行物理脱离（如外部 Cancel），强制执行
    if (!bHasUnbound)
    {
        ExecutePhysicalUnbind();
    }

    TargetVehicle = nullptr;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
