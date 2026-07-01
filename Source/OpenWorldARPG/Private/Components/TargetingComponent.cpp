// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/TargetableInterface.h"

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTargetingComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!OwnerCharacter) return;

    if (!OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    AActor* BestTarget = FindBestTarget();

    if (BestTarget != CurrentBestTarget)
    {
        AActor* OldTarget = CurrentBestTarget;
        CurrentBestTarget = BestTarget;

        // 通过接口调用旧目标的 OnClearAsTarget
        if (OldTarget && OldTarget->Implements<UTargetableInterface>())
        {
            ITargetableInterface::Execute_OnClearAsTarget(OldTarget);
        }

        // 通过接口调用新目标的 OnSetAsTarget
        if (CurrentBestTarget && CurrentBestTarget->Implements<UTargetableInterface>())
        {
            ITargetableInterface::Execute_OnSetAsTarget(CurrentBestTarget);
        }

        if (OnBestTargetChanged.IsBound())
        {
            OnBestTargetChanged.Broadcast(OldTarget, CurrentBestTarget);
        }
    }
}

void UTargetingComponent::AddTarget(AActor* NewTarget)
{
    if (NewTarget && !AvailableTargets.Contains(NewTarget))
    {
        AvailableTargets.Add(NewTarget);
    }
}

void UTargetingComponent::RemoveTarget(AActor* TargetToRemove)
{
    if (TargetToRemove)
    {
        AvailableTargets.Remove(TargetToRemove);

        // 安全保护
        if (CurrentBestTarget == TargetToRemove)
        {
            AActor* OldTarget = CurrentBestTarget;
            CurrentBestTarget = nullptr;

            // 通过接口调用旧目标的 OnClearAsTarget
            if (OldTarget && OldTarget->Implements<UTargetableInterface>())
            {
                ITargetableInterface::Execute_OnClearAsTarget(OldTarget);
            }

            if (OnBestTargetChanged.IsBound())
            {
                OnBestTargetChanged.Broadcast(OldTarget, nullptr);
            }
        }
    }
}

float UTargetingComponent::CalculateAngleDot(AActor* Target) const
{
    if (!OwnerCharacter || !Target) return -1.0f;

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return -1.0f;

    FVector CameraForward = PC->GetControlRotation().Vector();
    FVector DirectionToTarget = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();

    return FVector::DotProduct(CameraForward, DirectionToTarget);
}

AActor* UTargetingComponent::FindBestTarget() const
{
    AActor* BestTarget = nullptr;
    float BestDotValue = MinDotThreshold;

    for (AActor* Target : AvailableTargets)
    {
        if (!IsValid(Target)) continue;

        float CurrentDot = CalculateAngleDot(Target);

        if (CurrentDot > BestDotValue)
        {
            BestDotValue = CurrentDot;
            BestTarget = Target;
        }
    }

    return BestTarget;
}
