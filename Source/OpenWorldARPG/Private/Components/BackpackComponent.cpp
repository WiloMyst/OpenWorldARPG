// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/BackpackComponent.h"
#include "Items/ItemBase.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UBackpackComponent::UBackpackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void UBackpackComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        InventorySubsystem = GI->GetSubsystem<UInventoryManagerSubsystem>();
        if (InventorySubsystem)
        {
            InventorySubsystem->OnItemDropped.AddDynamic(this, &UBackpackComponent::HandleOnItemDropped);
        }
    }
}

void UBackpackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector StartLoc = OwnerActor->GetActorLocation();
    FVector EndLoc = StartLoc;
    float SphereRadius = 150.0f;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    FHitResult HitResult;
    bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
        this, StartLoc, EndLoc, SphereRadius, ObjectTypes,
        false, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);

    CurrentPickableItem = bHit && HitResult.GetActor() ? Cast<AItemBase>(HitResult.GetActor()) : nullptr;
}

bool UBackpackComponent::IsCharacterInStandby() const
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

int32 UBackpackComponent::GetOwnerCharacterID() const
{
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (PlayerChar)
    {
        // 使用角色 Tag 的哈希值作为 CharacterID
        // 实际项目中应使用 CharacterManagerSubsystem 分配的 ID
        return PlayerChar->GetCharacterTag().GetTagName().GetNumber();
    }
    return -1;
}

void UBackpackComponent::PickUpItem()
{
    if (IsCharacterInStandby() || !CurrentPickableItem.IsValid() || !InventorySubsystem) return;

    AItemBase* PickableItem = CurrentPickableItem.Get();
    int32 ItemID = PickableItem->ItemID;
    int32 Amount = PickableItem->ItemAmount;

    InventorySubsystem->AddItem(ItemID, Amount);
    PickableItem->Destroy();
    CurrentPickableItem = nullptr;
}

void UBackpackComponent::DropItemByGUID(FGuid ItemGUID, int32 DropAmount)
{
    if (!InventorySubsystem) return;
    InventorySubsystem->RemoveItemByGUID(ItemGUID, DropAmount);
}

void UBackpackComponent::DropItem(int32 DropIndex, int32 DropAmount)
{
    if (!InventorySubsystem) return;
    InventorySubsystem->RemoveItemByIndex(DropIndex, DropAmount);
}

bool UBackpackComponent::UseItemByGUID(FGuid ItemGUID, int32 UseAmount)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->UseItem(ItemGUID, GetOwnerCharacterID(), UseAmount);
}

bool UBackpackComponent::EquipItemByGUID(FGuid ItemGUID)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->EquipItem(ItemGUID, GetOwnerCharacterID());
}

bool UBackpackComponent::UnequipItemByGUID(FGuid ItemGUID)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->UnequipItem(ItemGUID);
}

void UBackpackComponent::HandleOnItemDropped(int32 ItemID, int32 DroppedAmount)
{
    if (IsCharacterInStandby()) return;
    SpawnDroppedItem(ItemID, DroppedAmount);
}

void UBackpackComponent::SpawnDroppedItem(int32 ItemID, int32 DroppedAmount)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector OwnerLoc = OwnerActor->GetActorLocation();
    FVector OwnerForward = OwnerActor->GetActorForwardVector();
    FVector SpawnLocation = OwnerLoc + (OwnerForward * 100.0f);
    FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.Instigator = Cast<APawn>(OwnerActor);

    AItemBase* DroppedItem = GetWorld()->SpawnActor<AItemBase>(AItemBase::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
    if (DroppedItem)
    {
        DroppedItem->InitializeItem(ItemID, DroppedAmount);
        FVector LinearVelocity = (OwnerForward * 300.0f) + FVector(0.0f, 0.0f, 300.0f);
        DroppedItem->ApplyThrowPhysics(LinearVelocity);
    }
}
