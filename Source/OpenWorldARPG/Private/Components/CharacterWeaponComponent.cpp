// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/CharacterWeaponComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Weapons/WeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"

// =====================================================================
// 生命周期与初始化
// =====================================================================

UCharacterWeaponComponent::UCharacterWeaponComponent()
{
    // 开启 Tick，用于检测移动状态以自动收起武器
    PrimaryComponentTick.bCanEverTick = true;
}

void UCharacterWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    // 缓存持有该组件的角色，提升后续调用的性能
    CachedCharacter = Cast<APlayerCharacter>(GetOwner());
}

// =====================================================================
// 核心武器状态机
// =====================================================================

void UCharacterWeaponComponent::InitializeCharacterWeapon()
{
    if (!CachedCharacter) return;

    // 1. 从角色直接获取武器蓝图
    TSubclassOf<AWeaponBase> WeaponClass = CachedCharacter->GetWeaponBlueprint();
    if (!WeaponClass) return;

    // 2. 获取武器背负挂载点 (悬浮弹簧臂插槽)
    USceneComponent* RestSocket = CachedCharacter->GetWeaponRestSocket();
    if (!RestSocket) return;

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
        // 挂载到弹簧臂末端，不需要提供 SocketName
        CharacterWeapon->AttachToComponent(RestSocket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        bIsWeaponStowed = true;
    }
}

void UCharacterWeaponComponent::WeaponToHand()
{
    if (!CharacterWeapon || !CachedCharacter || !CachedCharacter->GetMesh()) return;

    // 拔出武器：挂载到角色的骨骼网格体对应的手部插槽 (由蓝图配置的 HandSocketName 决定)
    CharacterWeapon->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, HandSocketName);

    // 更新状态机
    bIsWeaponStowed = false;
}

void UCharacterWeaponComponent::WeaponToBack()
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

void UCharacterWeaponComponent::SetWeaponHidden(bool bHidden)
{
    if (CharacterWeapon)
    {
        CharacterWeapon->SetActorHiddenInGame(bHidden);
    }
}

// =====================================================================
// 状态轮询 (自动收回逻辑)
// =====================================================================

void UCharacterWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 安全检查：如果没有缓存角色、没有武器，或者武器已经在背上，直接跳过 (极低性能消耗)
    if (!CachedCharacter || !CharacterWeapon || bIsWeaponStowed) return;

    // 获取角色当前的移动输入向量长度平方
    FVector InputVector = CachedCharacter->GetLastMovementInputVector();

    // 判定：如果角色正在移动 (超过蓝图配置的阈值)
    if (InputVector.SizeSquared() > MovementInputThreshold)
    {
        UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
        if (ASC && PreventStowTags.IsValid())
        {
            // 核心判定：只要角色身上含有 PreventStowTags (如攻击、瞄准、施法) 里的任意一个标签，就阻止收起
            if (ASC->HasAnyMatchingGameplayTags(PreventStowTags))
            {
                return;
            }
        }

        // 没有任何阻止收起的标签，把武器放回背上
        WeaponToBack();
    }
}