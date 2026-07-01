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

    // 网络同步：组件需要复制才能让 Server RPC 工作
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

    // 仅在本地控制端执行交互检测（避免服务器为所有客户端角色做检测）
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn && !OwnerPawn->IsLocallyControlled()) return;

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector StartLoc = OwnerActor->GetActorLocation();
    FVector EndLoc = StartLoc;

    // 通用通道：覆盖掉落物(PhysicsBody)、动态物件(WorldDynamic)、载具(Vehicle)、角色(Pawn)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Vehicle));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    // 多体检测，支持多个交互对象重叠
    TArray<FHitResult> HitResults;
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this, StartLoc, EndLoc, InteractionRadius, ObjectTypes,
        false, ActorsToIgnore, EDrawDebugTrace::None, HitResults, true);

    // 遍历命中结果，通过接口筛选可交互对象
    TArray<AActor*> NewInteractableActors;
    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor) continue;

        // 接口化检测：不依赖任何具体类型
        if (HitActor->Implements<UInteractableInterface>())
        {
            // 调用接口方法确认是否可以交互
            if (IInteractableInterface::Execute_CanInteract(HitActor, Cast<ACharacter>(OwnerPawn)))
            {
                NewInteractableActors.Add(HitActor);
            }
        }
    }

    // 比对新旧列表是否发生变化
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

    // 仅当可交互对象列表发生改变时才广播
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

// --- 通用交互：多态分发 ---

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
