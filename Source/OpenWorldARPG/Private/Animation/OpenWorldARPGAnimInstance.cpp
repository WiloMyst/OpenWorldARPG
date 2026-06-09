// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/OpenWorldARPGAnimInstance.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Kismet/KismetMathLibrary.h"

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

        // 预构造 GameplayTag（避免每帧 RequestGameplayTag 的哈希查找开销）
    
    DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Dead"));
    UncontrollableTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Uncontrollable"));
}

void UOpenWorldARPGAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

        // GameThread 更新：快照 ASC 的 GameplayTag
    //
    // 为什么在这里做？
    // - ASC 的 HasTag / GetOwnedGameplayTags 不是线程安全的
    // - NativeThreadSafeUpdateAnimation 在 Worker Thread 执行
    // - 工业标准做法：主线程拉取快照 → 工作线程只读消费
    
    SnapshotGameplayTags.Reset();

    if (CachedASC.IsValid())
    {
        CachedASC->GetOwnedGameplayTags(SnapshotGameplayTags);
    }
}

void UOpenWorldARPGAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

        // Worker Thread 安全更新：只读取纯数据
    // 数据来源：
    // 1. CMC 物理状态 → 线程安全（CMC 物理阶段已写入）
    // 2. Character Velocity → 线程安全（由 CMC 每帧更新）
    // 3. ASC Tag 快照 → 主线程已拉取到 SnapshotGameplayTags
    
    const ACharacter* Character = CachedCharacter.Get();
    const UOpenWorldARPGCharacterMovementComponent* MoveComp = CachedMovementComp.Get();

    if (!Character || !MoveComp)
    {
        return;
    }

        // 1. 基础运动数据 (Locomotion)
    
    const FVector Velocity = Character->GetVelocity();
    const FVector VelocityXY(Velocity.X, Velocity.Y, 0.0f);

    GroundSpeed = VelocityXY.Size();
    VelocityZ = Velocity.Z;

    // bIsMoving：有加速度（输入）且速度超过阈值
    const FVector LastInput = MoveComp->GetLastInputVector();
    bIsMoving = (GroundSpeed > MoveSpeedThreshold) && !LastInput.IsNearlyZero(0.01f);

    // 本地空间速度方向角度（驱动方向混合空间）
    if (GroundSpeed > MoveSpeedThreshold)
    {
        const FVector LocalVelocity = Character->GetActorRotation().UnrotateVector(VelocityXY);
        LocalVelocityDirection = UKismetMathLibrary::MakeRotFromX(LocalVelocity).Yaw;
    }
    else
    {
        LocalVelocityDirection = 0.0f;
    }

        // 2. 移动状态数据 (从 CMC 直接读取)
    
    bIsGrounded = MoveComp->IsGrounded();
    bIsFalling = MoveComp->IsFalling();

        // 3. GAS Tag 快照 → bool 变量
    // 读取主线程已拉取的 SnapshotGameplayTags，工作线程零锁访问
    
    bIsDead = SnapshotGameplayTags.HasTag(DeadTag);
    bIsUncontrollable = SnapshotGameplayTags.HasTag(UncontrollableTag);
}
