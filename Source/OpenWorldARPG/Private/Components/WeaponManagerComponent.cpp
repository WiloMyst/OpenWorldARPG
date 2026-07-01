// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/WeaponManagerComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Weapons/WeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"

// --- 生命周期与初始化 ---

UWeaponManagerComponent::UWeaponManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedCharacter = Cast<APlayerCharacter>(GetOwner());
    UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] BeginPlay: Owner=%s, CachedCharacter=%s"), *GetNameSafe(GetOwner()), CachedCharacter ? *CachedCharacter->GetName() : TEXT("NULL"));
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

// --- 核心武器状态机 ---

void UWeaponManagerComponent::InitializeCharacterWeapon()
{
    // CachedCharacter 可能在 BeginPlay 之前调用时为空，主动获取一次
    if (!CachedCharacter)
    {
        CachedCharacter = Cast<APlayerCharacter>(GetOwner());
    }

    if (!CachedCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("[WeaponComp] InitializeCharacterWeapon: CachedCharacter is NULL even after GetOwner(), abort!"));
        return;
    }

    // 1. 从角色直接获取武器蓝图
    TSubclassOf<AWeaponBase> WeaponClass = CachedCharacter->GetWeaponBlueprint();
    if (!WeaponClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[WeaponComp] InitializeCharacterWeapon: WeaponClass is NULL (VisualDataAsset is %s)"),
            CachedCharacter->GetVisualDataAsset_Implementation() ? TEXT("Valid") : TEXT("NULL"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] InitializeCharacterWeapon: WeaponClass=%s"), *WeaponClass->GetName());

    // 2. 获取武器背负挂载点 (悬浮弹簧臂插槽)
    USceneComponent* RestSocket = CachedCharacter->GetWeaponRestSocket();
    if (!RestSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[WeaponComp] InitializeCharacterWeapon: WeaponRestSocket is NULL!"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] InitializeCharacterWeapon: RestSocket=%s"), *RestSocket->GetName());

    // 3. 配置生成参数并生成武器实体
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = CachedCharacter;
    SpawnParams.Instigator = CachedCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FTransform SpawnTransform = RestSocket->GetComponentTransform();
    CharacterWeapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);

    // 4. 将生成的武器挂载到背部
    if (CharacterWeapon)
    {
        CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        bIsWeaponStowed = true;
        UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] InitializeCharacterWeapon: Weapon spawned and attached to back. Weapon=%s, bHidden=%s"),
            *CharacterWeapon->GetName(),
            CharacterWeapon->IsHidden() ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[WeaponComp] InitializeCharacterWeapon: SpawnActor FAILED! WeaponClass=%s"), *WeaponClass->GetName());
        return;
    }

    // 5. 注册 GameplayTag 事件监听，改为事件驱动
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

        UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] InitializeCharacterWeapon: Registered GameplayTag events."));
    }
}

void UWeaponManagerComponent::WeaponToHand()
{
    if (!CharacterWeapon || !CachedCharacter || !CachedCharacter->GetMesh())
    {
        return;
    }

    // 拔出武器：挂载到角色的骨骼网格体对应的手部插槽 (由蓝图配置的 HandSocketName 决定)
    CharacterWeapon->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandSocketName);

    // 更新状态机
    bIsWeaponStowed = false;
}

void UWeaponManagerComponent::WeaponToBack()
{
    USceneComponent* RestSocket = CachedCharacter ? CachedCharacter->GetWeaponRestSocket() : nullptr;
    if (!CharacterWeapon || !RestSocket) return;

    // 1. 播放消散特效 (材质动画/粒子)
    CharacterWeapon->PlayWeaponDissolveFX();

    // 2. 将武器挂载回背部的弹簧臂悬浮插槽
    CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::KeepRelativeTransform);

    // 3. 播放生成特效
    CharacterWeapon->PlayWeaponSpawnFX();

    // 4. 更新状态机
    bIsWeaponStowed = true;
}

void UWeaponManagerComponent::SetWeaponHidden(bool bHidden)
{
    if (CharacterWeapon)
    {
        CharacterWeapon->SetActorHiddenInGame(bHidden);
        UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] SetWeaponHidden: bHidden=%s, Weapon=%s"), bHidden ? TEXT("true") : TEXT("false"), *CharacterWeapon->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] SetWeaponHidden: CharacterWeapon is NULL, cannot set hidden=%s"), bHidden ? TEXT("true") : TEXT("false"));
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

void UWeaponManagerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    DestroyCharacterWeapon();
    CachedASC = nullptr;
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UWeaponManagerComponent::OnMovementTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    UpdateWeaponState();
}

void UWeaponManagerComponent::OnPreventStowTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    UpdateWeaponState();
}
