// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_MountVehicleBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter.h"
#include "Vehicles/WheeledVehiclePawnBase.h"
#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/Character.h"

UGA_MountVehicleBase::UGA_MountVehicleBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    bHasExecutedMount = false;
}

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

    // --- Motion Warping：设置吸附目标 ---
    if (UMotionWarpingComponent* MotionWarping = PlayerChar->GetMotionWarpingComp())
    {
        const FTransform InteractionTransform = TargetVehicle->GetInteractionTargetTransform_Implementation();
        MotionWarping->AddOrUpdateWarpTargetFromLocation(WarpTargetName, InteractionTransform.GetLocation());
    }

    // --- 播放上车蒙太奇 ---
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
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        // 无蒙太奇配置，直接执行上车
        ExecuteMount();
    }
}

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
        APlayerController* BasePC = Cast<APlayerController>(PlayerChar->GetController());

        if (AOpenWorldPlayerController* OW_PC = Cast<AOpenWorldPlayerController>(BasePC))
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
    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }

    TargetVehicle = nullptr;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
