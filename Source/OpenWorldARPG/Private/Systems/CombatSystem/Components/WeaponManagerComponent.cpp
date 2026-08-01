// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Components/WeaponManagerComponent.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/CombatSystem/Weapons/WeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

UWeaponManagerComponent::UWeaponManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
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

void UWeaponManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, CharacterWeapon);
    DOREPLIFETIME(ThisClass, bIsWeaponStowed);
}

// --- 武器管理 ---

void UWeaponManagerComponent::InitializeCharacterWeapon()
{
    if (!CachedCharacter)
    {
        CachedCharacter = Cast<APlayerCharacter>(GetOwner());
    }
    if (!CachedCharacter) return;

    // 武器 Spawn 与 Tag 监听仅在服务器执行；客户端通过复制的 CharacterWeapon/bIsWeaponStowed 同步表现
    if (!GetOwner()->HasAuthority()) return;

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

void UWeaponManagerComponent::ApplyWeaponAttachState()
{
    if (!CharacterWeapon || !CachedCharacter) return;

    if (bIsWeaponStowed)
    {
        USceneComponent* RestSocket = CachedCharacter->GetWeaponRestSocket();
        if (RestSocket)
        {
            CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
    else if (CachedCharacter->GetMesh())
    {
        CharacterWeapon->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandSocketName);
    }
}

void UWeaponManagerComponent::OnRep_CharacterWeapon()
{
    // 客户端首次收到服务器复制的武器引用，按当前收/拔状态挂载
    if (CharacterWeapon && CachedCharacter)
    {
        ApplyWeaponAttachState();
    }
}

void UWeaponManagerComponent::OnRep_IsWeaponStowed()
{
    // 服务器收/拔状态变化后，客户端重新挂载武器到对应 Socket
    ApplyWeaponAttachState();
}

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
