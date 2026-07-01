// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/WeaponManagerComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Weapons/WeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"

UWeaponManagerComponent::UWeaponManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedCharacter = Cast<APlayerCharacter>(GetOwner());
}

// --- 生命周期 ---

void UWeaponManagerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    DestroyCharacterWeapon();
    CachedASC = nullptr;
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

// --- 武器管理 ---

void UWeaponManagerComponent::InitializeCharacterWeapon()
{
    if (!CachedCharacter)
    {
        CachedCharacter = Cast<APlayerCharacter>(GetOwner());
    }
    if (!CachedCharacter) return;

    TSubclassOf<AWeaponBase> WeaponClass = CachedCharacter->GetWeaponBlueprint();
    if (!WeaponClass) return;

    USceneComponent* RestSocket = CachedCharacter->GetWeaponRestSocket();
    if (!RestSocket) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = CachedCharacter;
    SpawnParams.Instigator = CachedCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FTransform SpawnTransform = RestSocket->GetComponentTransform();
    CharacterWeapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);

    if (CharacterWeapon)
    {
        CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        bIsWeaponStowed = true;
    }

    CachedASC = CachedCharacter->GetAbilitySystemComponent();
    if (CachedASC)
    {
        if (MovingTag.IsValid())
        {
            CachedASC->RegisterGameplayTagEvent(MovingTag).AddUObject(this, &UWeaponManagerComponent::OnMovementTagChanged);
        }

        for (const FGameplayTag& Tag : PreventStowTags)
        {
            if (Tag.IsValid())
            {
                CachedASC->RegisterGameplayTagEvent(Tag).AddUObject(this, &UWeaponManagerComponent::OnPreventStowTagChanged);
            }
        }
    }
}

void UWeaponManagerComponent::DestroyCharacterWeapon()
{
    if (CharacterWeapon)
    {
        CharacterWeapon->Destroy();
        CharacterWeapon = nullptr;
    }
}

void UWeaponManagerComponent::WeaponToHand()
{
    if (!CharacterWeapon || !CachedCharacter || !CachedCharacter->GetMesh()) return;

    CharacterWeapon->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandSocketName);
    bIsWeaponStowed = false;
}

void UWeaponManagerComponent::WeaponToBack()
{
    USceneComponent* RestSocket = CachedCharacter ? CachedCharacter->GetWeaponRestSocket() : nullptr;
    if (!CharacterWeapon || !RestSocket) return;

    CharacterWeapon->PlayWeaponDissolveFX();
    CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::KeepRelativeTransform);
    CharacterWeapon->PlayWeaponSpawnFX();
    bIsWeaponStowed = true;
}

void UWeaponManagerComponent::SetWeaponHidden(bool bHidden)
{
    if (CharacterWeapon)
    {
        CharacterWeapon->SetActorHiddenInGame(bHidden);
    }
}

// --- 内部逻辑 ---

void UWeaponManagerComponent::UpdateWeaponState()
{
    if (!CharacterWeapon || !CachedASC) return;

    bool bIsMoving = MovingTag.IsValid() && CachedASC->HasMatchingGameplayTag(MovingTag);
    bool bHasPreventStow = CachedASC->HasAnyMatchingGameplayTags(PreventStowTags);

    if (bIsMoving && !bHasPreventStow)
    {
        WeaponToBack();
    }
}

// --- Tag 回调 ---

void UWeaponManagerComponent::OnMovementTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    UpdateWeaponState();
}

void UWeaponManagerComponent::OnPreventStowTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    UpdateWeaponState();
}
