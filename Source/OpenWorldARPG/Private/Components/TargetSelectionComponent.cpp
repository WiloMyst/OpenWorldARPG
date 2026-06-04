// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/TargetSelectionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Characters/SelectableTargetActor.h"

UTargetSelectionComponent::UTargetSelectionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTargetSelectionComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UTargetSelectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

        // 直接调用旧目标的 OnClearAsTarget
        if (OldTarget)
        {
            if (ASelectableTargetActor* OldSelectable = Cast<ASelectableTargetActor>(OldTarget))
            {
                OldSelectable->OnClearAsTarget();
            }
        }

        // 直接调用新目标的 OnSetAsTarget
        if (CurrentBestTarget)
        {
            if (ASelectableTargetActor* NewSelectable = Cast<ASelectableTargetActor>(CurrentBestTarget))
            {
                NewSelectable->OnSetAsTarget();
            }
        }

        if (OnBestTargetChanged.IsBound())
        {
            OnBestTargetChanged.Broadcast(OldTarget, CurrentBestTarget);
        }
    }
}

void UTargetSelectionComponent::AddTarget(AActor* NewTarget)
{
    if (NewTarget && !AvailableTargets.Contains(NewTarget))
    {
        AvailableTargets.Add(NewTarget);
    }
}

void UTargetSelectionComponent::RemoveTarget(AActor* TargetToRemove)
{
    if (TargetToRemove)
    {
        AvailableTargets.Remove(TargetToRemove);

        // 安全保护
        if (CurrentBestTarget == TargetToRemove)
        {
            AActor* OldTarget = CurrentBestTarget;
            CurrentBestTarget = nullptr;

            // 直接调用旧目标的 OnClearAsTarget
            if (ASelectableTargetActor* OldSelectable = Cast<ASelectableTargetActor>(OldTarget))
            {
                OldSelectable->OnClearAsTarget();
            }

            if (OnBestTargetChanged.IsBound())
            {
                OnBestTargetChanged.Broadcast(OldTarget, nullptr);
            }
        }
    }
}

float UTargetSelectionComponent::CalculateAngleDot(AActor* Target) const
{
    if (!OwnerCharacter || !Target) return -1.0f;

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return -1.0f;

    FVector CameraForward = PC->GetControlRotation().Vector();
    FVector DirectionToTarget = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();

    return FVector::DotProduct(CameraForward, DirectionToTarget);
}

AActor* UTargetSelectionComponent::FindBestTarget() const
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