// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/OpenWorldARPGAnimInstance.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

UOpenWorldARPGAnimInstance::UOpenWorldARPGAnimInstance()
    : Super()
{
}

void UOpenWorldARPGAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // 缓存通用组件指针（仅此一次，后续零开销访问）
    // 使用 TWeakObjectPtr：Worker Thread 访问时若对象已销毁则安全失效
    if (ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner()))
    {
        CachedCharacter = Character;

        // 缓存自定义移动组件
        CachedMovementComp = Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>();

        // 缓存 ASC：通过 IAbilitySystemInterface 获取
        if (Character->Implements<UAbilitySystemInterface>())
        {
            CachedASC = Cast<IAbilitySystemInterface>(Character)->GetAbilitySystemComponent();
        }
    }
}

void UOpenWorldARPGAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // ================================================================
    // GameThread 全面快照
    // ================================================================
    // 所有需要从 Actor / MovementComponent 读取的数据必须在此完成。
    // ASC 的 HasTag / GetOwnedGameplayTags 也不是线程安全的。
    // AActor::GetVelocity / GetActorRotation / GetActorLocation 等均非线程安全。
    // CMC 的 IsGrounded / IsFalling / GetLastInputVector 同理。
    // 工作线程只读取以下 Snapshot... 变量进行纯数学运算。

    SnapshotGameplayTags.Reset();

    const ACharacter* Character = CachedCharacter.Get();
    const UOpenWorldARPGCharacterMovementComponent* MoveComp = CachedMovementComp.Get();

    if (!Character || !MoveComp)
    {
        return;
    }

    // --- 通用物理快照（子类共享）---

    SnapshotVelocity = Character->GetVelocity();
    SnapshotActorRotation = Character->GetActorRotation();
    SnapshotActorLocation = Character->GetActorLocation();
    SnapshotActorForwardVector = Character->GetActorForwardVector();
    SnapshotActorRightVector = Character->GetActorRightVector();
    SnapshotLastInputVector = MoveComp->GetLastInputVector();
    bSnapshotIsGrounded = MoveComp->IsGrounded();
    bSnapshotIsFalling = MoveComp->IsFalling();

    // --- GAS Tag 快照 ---

    if (CachedASC.IsValid())
    {
        CachedASC->GetOwnedGameplayTags(SnapshotGameplayTags);
    }
}

void UOpenWorldARPGAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    // ================================================================
    // Worker Thread 纯数据计算
    // ================================================================
    // 严禁出现任何 Character->Get...() 或 MoveComp->...() 调用。
    // 所有数据来源均为 NativeUpdateAnimation 中写入的 Snapshot... 变量。

    // --- 1. 基础运动数据 (Locomotion) ---

    const FVector VelocityXY(SnapshotVelocity.X, SnapshotVelocity.Y, 0.0f);

    GroundSpeed = VelocityXY.Size();
    VelocityZ = SnapshotVelocity.Z;

    // 本地空间速度方向角度（驱动方向混合空间）
    if (GroundSpeed > MoveSpeedThreshold)
    {
        const FVector LocalVelocity = SnapshotActorRotation.UnrotateVector(VelocityXY);
        LocalVelocityDirection = FRotationMatrix::MakeFromX(LocalVelocity).Rotator().Yaw;
    }
    else
    {
        LocalVelocityDirection = 0.0f;
    }

    // --- 2. 移动状态数据（从主线程快照读取）---

    bIsGrounded = bSnapshotIsGrounded;
    bIsFalling = bSnapshotIsFalling;

    // --- 3. GAS Tag 快照 → bool 变量 ---

    bIsDead = SnapshotGameplayTags.HasTag(DeadTag);
}
