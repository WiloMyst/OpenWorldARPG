// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InteractionSystem/Components/InteractionComponent.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "Systems/VehicleSystem/Pawns/VehiclePawnBase.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
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
    if (IsCharacterInStandby())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] Interact abort: 角色处于 Standby 状态"));
        return;
    }
    if (CurrentInteractableActors.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] Interact abort: 交互列表为空"));
        return;
    }

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (!PlayerChar) return;

    AActor* TargetActor = nullptr;
    for (const TWeakObjectPtr<AActor>& WeakActor : CurrentInteractableActors)
    {
        AActor* Candidate = WeakActor.Get();
        if (Candidate && Candidate->Implements<UInteractableInterface>())
        {
            TargetActor = Candidate;
            break;
        }
    }

    if (TargetActor)
    {
        UE_LOG(LogTemp, Log, TEXT("[MountDiag] Interact: 找到目标 %s, 发送 Server_Interact"), *TargetActor->GetName());
        Server_Interact(TargetActor);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] Interact: 交互列表有 %d 个 Actor 但无实现 IInteractableInterface"), CurrentInteractableActors.Num());
    }
}

bool UInteractionComponent::Server_Interact_Validate(AActor* TargetActor)
{
    return IsValid(TargetActor);
}

void UInteractionComponent::Server_Interact_Implementation(AActor* TargetActor)
{
    if (!IsValid(TargetActor) || !TargetActor->Implements<UInteractableInterface>()) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (!PlayerChar) return;

    // 服务器侧二次校验交互距离，防止客户端伪造远程交互
    // 载具体积大，用 MaxEnterDistance（默认 300cm）；其他交互物用 1.5x InteractionRadius
    float AllowedDist = InteractionRadius * 1.5f;
    if (AVehiclePawnBase* Vehicle = Cast<AVehiclePawnBase>(TargetActor))
    {
        AllowedDist = Vehicle->GetMaxEnterDistance();
    }
    const float ActualDist = FVector::Dist(PlayerChar->GetActorLocation(), TargetActor->GetActorLocation());
    if (ActualDist > AllowedDist)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MountDiag] Server_Interact: 距离 %.1f 超过限制 %.1f, 拒绝"), ActualDist, AllowedDist);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[MountDiag] Server_Interact: 调用 OnInteract, 目标=%s"), *TargetActor->GetName());
    IInteractableInterface::Execute_OnInteract(TargetActor, PlayerChar);
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
