// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/ClimbingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

UClimbingComponent::UClimbingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UClimbingComponent::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
    {
        OwnerCharacter = Player;
        MovementComp = OwnerCharacter->GetCharacterMovement();
        CustomMovementComp = Player->GetCustomMovementComp();
        ASC = OwnerCharacter->GetAbilitySystemComponent();

        Player->OnPlayerMovementInput.AddDynamic(this, &UClimbingComponent::HandleOwnerMovementInput);
    }
}

void UClimbingComponent::TryClimb()
{
    if (!OwnerCharacter || !MovementComp || !ASC) return;

    EMovementMode CurrentMode = MovementComp->MovementMode;
    if (CurrentMode != MOVE_Walking && CurrentMode != MOVE_NavWalking && CurrentMode != MOVE_Falling && CurrentMode != MOVE_Flying)
    {
        return;
    }

    if (ASC->HasAnyMatchingGameplayTags(BlockClimbingTags))
    {
        return;
    }

    FHitResult ChestHit;
    FHitResult HeadHit;
    if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
    {
        if (ChestHit.GetActor() && ChestHit.GetActor()->ActorHasTag(UnclimbableActorTag))
        {
            return;
        }

        FVector LastInput = MovementComp->GetLastInputVector();
        float DotResult = FVector::DotProduct(LastInput, ChestHit.Normal);

        if (DotResult < MinInputDotProduct)
        {
            EnterClimb(ChestHit);
        }
    }
}

bool UClimbingComponent::PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit)
{
    if (!OwnerCharacter) return false;
    FVector Forward = OwnerCharacter->GetActorForwardVector();

    FVector ChestStart = OwnerCharacter->GetActorLocation() + TraceOffset;
    FVector ChestEnd = ChestStart + (Forward * TraceDistance);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);
    bool bChestHit = GetWorld()->LineTraceSingleByChannel(OutChestHit, ChestStart, ChestEnd, TraceChannel, Params);

    if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
    {
        FVector HeadStart = Mesh->GetSocketLocation(HeadSocketName) + TraceOffset;
        FVector HeadEnd = HeadStart + (Forward * TraceDistance);
        GetWorld()->LineTraceSingleByChannel(OutHeadHit, HeadStart, HeadEnd, TraceChannel, Params);
    }
    return bChestHit;
}

void UClimbingComponent::EnterClimb(const FHitResult& WallHit)
{
    if (!OwnerCharacter || !MovementComp) return;

    // 1. 切换到自定义攀爬模式
    MovementComp->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
    MovementComp->StopMovementImmediately();
    MovementComp->bOrientRotationToMovement = false;

    // 2. 通知 CMC 当前墙壁法线
    if (CustomMovementComp)
    {
        CustomMovementComp->SetClimbWallNormal(WallHit.Normal);
    }

    // 3. 发送 Gameplay Event
    if (EventClimbStartTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStartTag, FGameplayEventData());
    }

    // 4. 贴墙位移与旋转
    FRotator TargetRot = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);
    FRotator FinalRot = FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.0f);

    FVector TargetLoc = OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, WallSnapZOffset);

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("NoOp");
    LatentInfo.Linkage = 0;
    LatentInfo.UUID = FMath::Rand();

    UKismetSystemLibrary::MoveComponentTo(
        OwnerCharacter->GetCapsuleComponent(),
        TargetLoc,
        FinalRot,
        false,
        false,
        WallSnapTime,
        false,
        EMoveComponentAction::Move,
        LatentInfo
    );
}

void UClimbingComponent::ExitClimb()
{
    if (!OwnerCharacter || !MovementComp) return;

    // 1. 清除 CMC 攀爬状态
    if (CustomMovementComp)
    {
        CustomMovementComp->ClearClimbInput();
    }

    // 2. 恢复掉落模式
    MovementComp->SetMovementMode(MOVE_Falling);
    MovementComp->bOrientRotationToMovement = true;

    // 3. 修正角色朝向
    FRotator CurrentRot = OwnerCharacter->GetActorRotation();
    OwnerCharacter->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));

    // 4. 发送停止事件
    if (EventClimbStopTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStopTag, FGameplayEventData());
    }
}

void UClimbingComponent::CheckAndClimbUp()
{
    if (!OwnerCharacter || !ASC) return;

    if (ClimbingStateTag.IsValid() && !ASC->HasMatchingGameplayTag(ClimbingStateTag))
    {
        return;
    }

    FVector StartLoc = OwnerCharacter->GetActorLocation() + (OwnerCharacter->GetActorForwardVector() * 50.0f);
    StartLoc.Z += ClimbUpCheckHalfHeight;
    FVector EndLoc = StartLoc;

    FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(ClimbUpCheckRadius, ClimbUpCheckHalfHeight);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    FHitResult HitResult;
    bool bHit = GetWorld()->SweepSingleByChannel(HitResult, StartLoc, EndLoc, FQuat::Identity, TraceChannel, CapsuleShape, Params);

    if (!bHit)
    {
        DoClimbUp();
    }
}

void UClimbingComponent::DoClimbUp()
{
    if (!OwnerCharacter || !MovementComp) return;

    // 1. 开启状态锁，防止翻越期间玩家乱按方向键
    bIsClimbingUp = true;

    // 2. 立即退出攀爬模式，切换到飞行模式（无重力）
    //    这样蒙太奇结束后动画状态机不会回到攀爬姿态
    if (CustomMovementComp)
    {
        CustomMovementComp->ClearClimbInput();
    }

    SavedGravityScale = MovementComp->GravityScale;
    MovementComp->SetMovementMode(MOVE_Flying);
    MovementComp->GravityScale = 0.0f;
    MovementComp->bOrientRotationToMovement = false;

    // 3. 发送攀爬停止事件（移除攀爬状态Tag，防止动画回到攀爬状态）
    if (EventClimbStopTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStopTag, FGameplayEventData());
    }

    // 4. 播放翻越蒙太奇
    float AnimDuration = 0.5f;
    if (ClimbUpMontage)
    {
        AnimDuration = OwnerCharacter->PlayAnimMontage(ClimbUpMontage);
    }

    // 5. 计算翻越的目标位置：基于角色当前位置，向前和向上偏移
    FVector ForwardDir = OwnerCharacter->GetActorForwardVector();
    FVector UpDir = OwnerCharacter->GetActorUpVector();

    FVector TargetLoc = OwnerCharacter->GetActorLocation()
        + (ForwardDir * ClimbUpOffset.X)
        + (UpDir * ClimbUpOffset.Z);

    // 6. 设置延时回调：用 MoveComponentTo 平滑搬运胶囊体
    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("OnClimbUpFinished");
    LatentInfo.Linkage = 0;
    LatentInfo.UUID = FMath::Rand();

    UKismetSystemLibrary::MoveComponentTo(
        OwnerCharacter->GetCapsuleComponent(),
        TargetLoc,
        OwnerCharacter->GetActorRotation(),
        false, false,
        AnimDuration,
        false,
        EMoveComponentAction::Move,
        LatentInfo
    );
}

void UClimbingComponent::OnClimbUpFinished()
{
    // 1. 解开状态锁
    bIsClimbingUp = false;

    // 2. 恢复重力并切换到掉落模式，角色自然落地
    MovementComp->GravityScale = SavedGravityScale;
    MovementComp->SetMovementMode(MOVE_Falling);
    MovementComp->bOrientRotationToMovement = true;

    // 3. 修正角色朝向
    FRotator CurrentRot = OwnerCharacter->GetActorRotation();
    OwnerCharacter->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
}

void UClimbingComponent::HandleOwnerMovementInput(float InputX, float InputY)
{
    if (!MovementComp) return;

    // 如果正在播放翻越动画并位移，直接丢弃所有玩家输入
    if (bIsClimbingUp) return;

    EMovementMode CurrentMode = MovementComp->MovementMode;

    // 地面或掉落时尝试检测墙壁触发攀爬
    if (CurrentMode == MOVE_Walking || CurrentMode == MOVE_NavWalking || CurrentMode == MOVE_Falling)
    {
        TryClimb();
    }

    // 攀爬模式：将输入和墙壁检测委托给 CMC
    if (MovementComp->MovementMode == MOVE_Custom &&
        MovementComp->CustomMovementMode == static_cast<uint8>(ECustomMovementMode::Climbing))
    {
        if (CustomMovementComp)
        {
            // 1. 更新墙壁法线 (在预测位置做射线检测)
            FVector ActorLoc = OwnerCharacter->GetActorLocation();
            FVector PredictedLoc = ActorLoc
                + (OwnerCharacter->GetActorRightVector() * InputX * ClimbPredictOffset)
                + (OwnerCharacter->GetActorUpVector() * InputY * ClimbPredictOffset);

            // 检测预测位置是否有空间
            FCollisionShape SpaceShape = FCollisionShape::MakeCapsule(ClimbSpaceCheckRadius, ClimbSpaceCheckHalfHeight);
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(OwnerCharacter);

            FHitResult SpaceHit;
            bool bHitSpace = GetWorld()->SweepSingleByChannel(SpaceHit, PredictedLoc, PredictedLoc, FQuat::Identity, TraceChannel, SpaceShape, Params);

            if (!bHitSpace)
            {
                // 有空间，检测预测位置的墙壁法线
                FVector RelativeOffset = PredictedLoc - ActorLoc;
                FHitResult ChestHit, HeadHit;
                if (PerformClimbTraces(RelativeOffset, ChestHit, HeadHit))
                {
                    CustomMovementComp->SetClimbWallNormal(ChestHit.Normal);
                }
            }

            // 2. 将输入传递给 CMC (即使没检测到新法线也传输入，CMC 会用上一次的法线)
            CustomMovementComp->SetClimbInput(InputX, InputY);
        }

        // 3. 检测翻越
        CheckAndClimbUp();
    }
}
