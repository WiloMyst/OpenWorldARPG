// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MovementSystem/Components/BaseCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UBaseCharacterMovementComponent::UBaseCharacterMovementComponent()
{
}

void UBaseCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    DefaultGravityScale = GravityScale;
    DefaultAirControl = AirControl;
}

void UBaseCharacterMovementComponent::CacheOwnerReferences()
{
    CachedOwnerCharacter = Cast<ACharacter>(GetOwner());
    if (CachedOwnerCharacter)
    {
        CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedOwnerCharacter.Get());
    }
}

// ============================================================================
// 引擎重写
// ============================================================================

void UBaseCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

    // 移动状态 Tag（根据输入意愿）
    if (CachedASC && MovingTag.IsValid())
    {
        bool bHasMovementInput = Acceleration.Size2D() > MovingSpeedThreshold;
        if (bHasMovementInput)
            CachedASC->AddLooseGameplayTag(MovingTag);
        else
            CachedASC->RemoveLooseGameplayTag(MovingTag);
    }
}

void UBaseCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    // 离开 Falling 时恢复默认物理参数
    if (PreviousMovementMode == MOVE_Falling && MovementMode != MOVE_Falling)
    {
        GravityScale = DefaultGravityScale;
        AirControl = DefaultAirControl;
        bOrientRotationToMovement = true;
    }

    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    // GAS Tag 管理 + 基础物理参数
    if (CachedASC)
    {
        // 清除所有已知状态 Tag（子类可在 Override 中补充自定义 Tag）
        if (AirborneTag.IsValid())      CachedASC->RemoveLooseGameplayTag(AirborneTag);
        if (FallingTag.IsValid())       CachedASC->RemoveLooseGameplayTag(FallingTag);
        if (SwimmingTag.IsValid())      CachedASC->RemoveLooseGameplayTag(SwimmingTag);
        if (FastSwimmingTag.IsValid())  CachedASC->RemoveLooseGameplayTag(FastSwimmingTag);
        if (MovingTag.IsValid())        CachedASC->RemoveLooseGameplayTag(MovingTag);

        if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
        {
            AirControl = 0.0f;
            RotationRate = GroundedRotationRate;
            bOrientRotationToMovement = true;
        }
        else if (MovementMode == MOVE_Falling)
        {
            if (AirborneTag.IsValid()) CachedASC->AddLooseGameplayTag(AirborneTag);
            if (FallingTag.IsValid())  CachedASC->AddLooseGameplayTag(FallingTag);

            AirControl = FallingAirControl;
            RotationRate = FallingRotationRate;
            bOrientRotationToMovement = true;
        }
    }

    OnMovementModeChangedDelegate.Broadcast(PreviousMovementMode, MovementMode.GetValue(), PreviousCustomMode, CustomMovementMode);
}
