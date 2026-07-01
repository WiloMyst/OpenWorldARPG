// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_UnmountVehicleBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter.h"
#include "GameFramework/Character.h"

UGA_UnmountVehicleBase::UGA_UnmountVehicleBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_UnmountVehicleBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
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
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_UnmountVehicleBase::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_UnmountVehicleBase::OnMontageBlendOut()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_UnmountVehicleBase::OnMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_UnmountVehicleBase::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_UnmountVehicleBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
