// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/NpcAnimInstance.h"
#include "Characters/AI/NpcCharacter.h"
#include "Kismet/GameplayStatics.h"

UNPCAnimInstance::UNPCAnimInstance()
    : Super()
{
}

void UNPCAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // NPC 极简初始化：不需要额外缓存
    // LookAt 目标通过主线程每帧查找玩家位置
}

void UNPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // ================================================================
    // GameThread 快照：玩家位置（用于 IK 盯防）
    // ================================================================
    // UGameplayStatics::GetPlayerCharacter 不是线程安全的。
    // 基类 NativeUpdateAnimation 已快照通用物理数据（ActorLocation 等），
    // 此处直接复用基类快照计算距离。

    SnapshotLookAtTargetLocation = FVector::ZeroVector;
    bSnapshotHasLookAtTarget = false;
    SnapshotEmotionState = 0;

    // 使用基类快照 SnapshotActorLocation 计算距离
    if (const ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
    {
        const float Distance = FVector::Dist(SnapshotActorLocation, PlayerChar->GetActorLocation());

        // 仅在玩家距离较近时启用 LookAt（避免远距离无意义的 IK 计算）
        if (Distance < 800.0f)
        {
            // LookAt 目标点：玩家头部位置（近似为角色位置 + 半身高）
            SnapshotLookAtTargetLocation = PlayerChar->GetActorLocation() + FVector(0.0f, 0.0f, PlayerChar->GetDefaultHalfHeight() * 2.0f);
            bSnapshotHasLookAtTarget = true;
        }
    }

    // TODO: 当对话系统集成后，从对话系统快照情绪状态
    // SnapshotEmotionState = DialogueComponent->GetCurrentEmotionState();
}

void UNPCAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // 基类先更新通用数据
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    // ================================================================
    // Worker Thread 纯数据计算
    // ================================================================
    // 严禁出现任何 Character->Get...() 调用。
    // 使用基类快照 SnapshotActorLocation / SnapshotActorForwardVector。

    // --- 1. LookAt 目标位置（主线程快照）---

    if (bSnapshotHasLookAtTarget)
    {
        LookAtTargetLocation = SnapshotLookAtTargetLocation;
    }
    else
    {
        // 无目标时，LookAt 位置设为角色正前方（自然朝向）
        // 使用基类快照，避免工作线程访问非线程安全数据
        LookAtTargetLocation = SnapshotActorLocation + SnapshotActorForwardVector * 100.0f;
    }

    // --- 2. 情绪状态（主线程快照）---

    CurrentEmotionState = SnapshotEmotionState;
}
