// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/InteractionComponent.h"
#include "Interfaces/InteractableInterface.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn && !OwnerPawn->IsLocallyControlled()) return;

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector StartLoc = OwnerActor->GetActorLocation();
    FVector EndLoc = StartLoc;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Vehicle));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    TArray<FHitResult> HitResults;
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this, StartLoc, EndLoc, InteractionRadius, ObjectTypes,
        false, ActorsToIgnore, EDrawDebugTrace::None, HitResults, true);

    TArray<AActor*> NewInteractableActors;
    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor) continue;

        if (HitActor->Implements<UInteractableInterface>())
        {
            if (IInteractableInterface::Execute_CanInteract(HitActor, Cast<ACharacter>(OwnerPawn)))
            {
                NewInteractableActors.Add(HitActor);
            }
        }
    }

    bool bChanged = false;
    if (NewInteractableActors.Num() != CurrentInteractableActors.Num())
    {
        bChanged = true;
    }
    else
    {
        for (int32 i = 0; i < NewInteractableActors.Num(); ++i)
        {
            if (NewInteractableActors[i] != CurrentInteractableActors[i].Get())
            {
                bChanged = true;
                break;
            }
        }
    }

    if (bChanged)
    {
        CurrentInteractableActors.Empty();
        for (AActor* Actor : NewInteractableActors)
        {
            CurrentInteractableActors.Add(Actor);
        }
        OnInteractableListChangedDelegate.Broadcast(NewInteractableActors);
    }
}

bool UInteractionComponent::IsCharacterInStandby() const
{
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (PlayerChar)
    {
        if (UAbilitySystemComponent* ASC = PlayerChar->GetAbilitySystemComponent())
        {
            FGameplayTag StandbyTag = PlayerChar->GetStandbyStateTag();
            return StandbyTag.IsValid() && ASC->HasMatchingGameplayTag(StandbyTag);
        }
    }
    return false;
}

void UInteractionComponent::Interact()
{
    if (IsCharacterInStandby() || CurrentInteractableActors.IsEmpty()) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (!PlayerChar) return;

    for (TWeakObjectPtr<AActor> WeakActor : CurrentInteractableActors)
    {
        AActor* TargetActor = WeakActor.Get();
        if (TargetActor && TargetActor->Implements<UInteractableInterface>())
        {
            IInteractableInterface::Execute_OnInteract(TargetActor, PlayerChar);
            break;
        }
    }
}

TArray<AActor*> UInteractionComponent::GetCurrentInteractableActors() const
{
    TArray<AActor*> Result;
    for (TWeakObjectPtr<AActor> WeakActor : CurrentInteractableActors)
    {
        if (WeakActor.IsValid())
        {
            Result.Add(WeakActor.Get());
        }
    }
    return Result;
}
