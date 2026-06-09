// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/ClimbingComponent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Interfaces/ARPGCharacterInterface.h"
#include "Data/CharacterDataAsset.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

UClimbingComponent::UClimbingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UClimbingComponent::BeginPlay()
{
    Super::BeginPlay();

    // 面向接口编程：Owner 统一视为 ACharacter，不再强转 APlayerCharacter
    // 组件可复用于怪物/NPC，只需实现 IARPGCharacterInterface 即可提供数据
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        OwnerCharacter = Character;
        MovementComp = Character->GetCharacterMovement();
        CustomMovementComp = Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>();
        ASC = Character->FindComponentByClass<UAbilitySystemComponent>();

        // 通过接口获取项目特定数据 (CharacterDataAsset)
        if (Character->GetClass()->ImplementsInterface(UARPGCharacterInterface::StaticClass()))
        {
            if (UCharacterDataAsset* DataAsset = IARPGCharacterInterface::Execute_GetCharacterDataAsset(Character))
            {
                CharacterData = DataAsset;
            }
        }

        // 解耦输入：不再依赖 APlayerCharacter::OnPlayerMovementInput 委托
        // 改为低频定时器轮询 CMC 的 LastInputVector，任何 ACharacter 都可复用
        GetWorld()->GetTimerManager().SetTimer(
            InputDetectionTimerHandle,
            this,
            &UClimbingComponent::OnInputDetectionTick,
            0.05f, // 20Hz 输入检测频率
            true,
            0.0f
        );
    }
}

// --- 核心接口 ---

void UClimbingComponent::TryClimb()
{
    if (!OwnerCharacter || !MovementComp || !ASC) return;

    // 只在地面或下落状态下允许尝试攀爬
    if (CustomMovementComp && !CustomMovementComp->IsGrounded() && !CustomMovementComp->IsFalling()) return;

    if (ASC->HasAnyMatchingGameplayTags(BlockClimbingTags)) return;

    FHitResult ChestHit;
    FHitResult HeadHit;
    if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
    {
        if (ChestHit.GetActor() && ChestHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return;
        if (!IsWallClimbable(ChestHit.Normal)) return;

        // 从引擎输入管线读取方向
        FVector LastInput = MovementComp->GetLastInputVector();
        float DotResult = FVector::DotProduct(LastInput, ChestHit.Normal);

        if (DotResult < MinInputDotProduct)
        {
            EnterClimb(ChestHit);
        }
    }
}

void UClimbingComponent::ExitClimb()
{
    if (!OwnerCharacter || !MovementComp) return;

    // 停止攀爬检测定时器
    GetWorld()->GetTimerManager().ClearTimer(ClimbDetectionTimerHandle);

    if (CustomMovementComp)
    {
        CustomMovementComp->ClearClimbState();
    }

    // CMC 的 SetMovementMode(MOVE_Falling) 会触发 OnMovementModeChanged，
    // 自动广播事件和更新 GAS Tag，无需 FSM 中转
    MovementComp->SetMovementMode(MOVE_Falling);
    MovementComp->bOrientRotationToMovement = true;

    FRotator CurrentRot = OwnerCharacter->GetActorRotation();
    OwnerCharacter->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));

    if (EventClimbStopTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStopTag, FGameplayEventData());
    }

    CurrentWallNormal = FVector::ZeroVector;
    bCMCHitWall = false;
    CMCHitWallNormal = FVector::ZeroVector;
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

    CurrentWallNormal = WallHit.Normal;
    bCMCHitWall = false;
    CMCHitWallNormal = FVector::ZeroVector;

    // 1. 先清零速度，再切换到攀爬模式
    //    StopMovementImmediately() 内部会调用 SetMovementMode(MOVE_Walking)，
    //    如果放在 SetMovementMode(MOVE_Custom) 之后，会覆盖刚设置的攀爬模式，
    //    导致 FSM 检测到模式不匹配而切回 Falling，形成攀爬/下落鬼畜循环
    MovementComp->Velocity = FVector::ZeroVector;
    MovementComp->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Climbing));
    MovementComp->bOrientRotationToMovement = false;

    // 2. 通知 CMC 当前墙壁法线
    if (CustomMovementComp)
    {
        CustomMovementComp->SetClimbWallNormal(WallHit.Normal);
    }

    // 3. CMC 的 SetMovementMode(MOVE_Custom, Climbing) 会触发 OnMovementModeChanged，
    // 自动广播事件，无需 FSM 中转

    // 4. 发送 Gameplay Event
    if (EventClimbStartTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStartTag, FGameplayEventData());
    }

    // 5. 计算吸附目标 (替代 MoveComponentTo)
    //    吸附由 CMC 的 PhysClimbing 在物理安全框架下执行 VInterpTo
    FRotator TargetRot = UKismetMathLibrary::MakeRotFromX(-WallHit.Normal);
    FRotator FinalRot = FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.0f);
    FVector TargetLoc = OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, WallSnapZOffset);

    if (CustomMovementComp)
    {
        CustomMovementComp->SetClimbSnapTarget(TargetLoc, FinalRot, WallSnapTime);
    }

    // 6. 启动攀爬低频检测定时器
    GetWorld()->GetTimerManager().SetTimer(
        ClimbDetectionTimerHandle,
        this,
        &UClimbingComponent::OnClimbDetectionTick,
        ClimbDetectionInterval,
        true,
        0.0f
    );
}

// --- 下落转攀爬 ---

void UClimbingComponent::CheckFallingToClimb()
{
    if (!OwnerCharacter || !MovementComp || !ASC) return;

    if (ASC->HasAnyMatchingGameplayTags(BlockClimbingTags)) return;

    // 从引擎输入管线读取方向
    FVector LastInput = MovementComp->GetLastInputVector();
    if (LastInput.IsNearlyZero()) return;

    FHitResult ChestHit;
    FHitResult HeadHit;
    if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
    {
        if (ChestHit.GetActor() && ChestHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return;
        if (!IsWallClimbable(ChestHit.Normal)) return;

        float DotResult = FVector::DotProduct(LastInput, ChestHit.Normal);
        if (DotResult < MinInputDotProduct)
        {
            GetWorld()->GetTimerManager().ClearTimer(FallingToClimbTimerHandle);
            EnterClimb(ChestHit);
        }
    }
}

bool UClimbingComponent::IsWallClimbable(const FVector& WallNormal) const
{
    return FMath::Abs(WallNormal.Z) < 0.2f;
}

// --- 攀爬转地面 ---

void UClimbingComponent::CheckClimbToGround()
{
    if (!OwnerCharacter) return;

    FVector Start = OwnerCharacter->GetActorLocation();
    float CapsuleHalfHeight = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector End = Start - FVector(0.0f, 0.0f, GroundDetectDistance + CapsuleHalfHeight);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, TraceChannel, Params);

    if (bHit && HitResult.bBlockingHit)
    {
        // 计算角色双脚离地高度
        float FeetHeightAboveGround = HitResult.Distance - CapsuleHalfHeight;

        // 只有双脚离地超过最小高度时才触发攀爬转地面
        // 防止在墙根处刚进入攀爬就被判定为"已到地面"而退出，导致鬼畜
        if (FeetHeightAboveGround < MinClimbHeightAboveGround)
        {
            return;
        }
        GetWorld()->GetTimerManager().ClearTimer(ClimbDetectionTimerHandle);

        if (CustomMovementComp)
        {
            CustomMovementComp->ClearClimbState();
        }

        // CMC 的 SetMovementMode(MOVE_Walking) 会触发 OnMovementModeChanged，
        // 自动广播事件和更新 GAS Tag，无需 FSM 中转
        MovementComp->SetMovementMode(MOVE_Walking);
        MovementComp->bOrientRotationToMovement = true;

        FRotator CurrentRot = OwnerCharacter->GetActorRotation();
        OwnerCharacter->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));

        if (EventClimbStopTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStopTag, FGameplayEventData());
        }

        CurrentWallNormal = FVector::ZeroVector;
        bCMCHitWall = false;
        CMCHitWallNormal = FVector::ZeroVector;
    }
}

// --- 墙角检测与过渡 ---

void UClimbingComponent::CheckCornerTransition()
{
    if (!OwnerCharacter || !CustomMovementComp || CurrentWallNormal.IsNearlyZero()) return;

    // 从引擎输入管线读取横向输入方向
    FVector LastInput = MovementComp->GetLastInputVector();
    if (LastInput.IsNearlyZero()) return;

    // 计算横向输入分量 (角色右方向上的投影)
    FVector ActorRight = OwnerCharacter->GetActorRightVector();
    float LateralInput = FVector::DotProduct(LastInput, ActorRight);
    if (FMath::IsNearlyZero(LateralInput)) return;

    if (CustomMovementComp && CustomMovementComp->IsInCornerTransition()) return;

    FVector ActorLocation = OwnerCharacter->GetActorLocation();
    FVector SideDir = ActorRight * FMath::Sign(LateralInput);
    FVector SweepStart = ActorLocation;
    FVector SweepEnd = ActorLocation + SideDir * CornerSweepDistance;

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(CornerSweepRadius);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    FHitResult SweepHit;
    bool bSweepHit = GetWorld()->SweepSingleByChannel(
        SweepHit, SweepStart, SweepEnd, FQuat::Identity, TraceChannel, SphereShape, Params
    );

    if (bSweepHit && SweepHit.bBlockingHit)
    {
        if (SweepHit.GetActor() && SweepHit.GetActor()->ActorHasTag(UnclimbableActorTag)) return;
        if (!IsWallClimbable(SweepHit.Normal)) return;

        float NormalDot = FVector::DotProduct(CurrentWallNormal, SweepHit.Normal);

        if (NormalDot < 0.0f)
        {
            HandleConvexCorner(SweepHit);
        }
        else if (NormalDot < 0.99f)
        {
            HandleConcaveCorner(SweepHit);
        }
    }
}

void UClimbingComponent::HandleConvexCorner(const FHitResult& NewWallHit)
{
    if (!OwnerCharacter || !CustomMovementComp) return;

    FVector CornerPoint = NewWallHit.Location;
    FVector TargetLocation = CalculateConvexTargetLocation(CornerPoint, CurrentWallNormal, NewWallHit.Normal);

    // SetCornerTransitionTarget 内部会切换 CustomMovementMode 为 ClimbingCornerTransition
    // CMC 的 SetMovementMode 会触发 OnMovementModeChanged，自动广播事件，无需 FSM 中转
    CustomMovementComp->SetCornerTransitionTarget(TargetLocation, NewWallHit.Normal, ECornerType::Convex);
}

void UClimbingComponent::HandleConcaveCorner(const FHitResult& NewWallHit)
{
    if (!OwnerCharacter || !CustomMovementComp) return;

    FVector TargetLocation = CalculateConcaveTargetLocation(NewWallHit.Location, NewWallHit.Normal);

    // SetCornerTransitionTarget 内部会切换 CustomMovementMode 为 ClimbingCornerTransition
    // CMC 的 SetMovementMode 会触发 OnMovementModeChanged，自动广播事件，无需 FSM 中转
    CustomMovementComp->SetCornerTransitionTarget(TargetLocation, NewWallHit.Normal, ECornerType::Concave);
}

FVector UClimbingComponent::CalculateConvexTargetLocation(const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const
{
    FVector Bisector = (CurrentNormal + NewNormal).GetSafeNormal();
    FVector TargetLoc = CornerPoint + Bisector * ConvexArcRadius;

    if (OwnerCharacter)
    {
        TargetLoc.Z = OwnerCharacter->GetActorLocation().Z;
    }

    return TargetLoc;
}

FVector UClimbingComponent::CalculateConcaveTargetLocation(const FVector& NewWallHitLocation, const FVector& NewWallNormal) const
{
    FVector TargetLoc = NewWallHitLocation + NewWallNormal * ConcaveSnapDistance;

    if (OwnerCharacter)
    {
        TargetLoc.Z = OwnerCharacter->GetActorLocation().Z;
    }

    return TargetLoc;
}

void UClimbingComponent::OnCornerTransitionFinished()
{
    // CMC 的 ClearCornerTransition 会将 CustomMovementMode 恢复为 Climbing，
    // 触发 OnMovementModeChanged 自动广播事件，无需 FSM 中转
}

// --- 翻越 (CMC PhysClimbUp 驱动位移，蒙太奇驱动动画) ---

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
    if (!OwnerCharacter || !MovementComp || !CustomMovementComp) return;

    bIsClimbingUp = true;

    // 停止攀爬检测定时器
    GetWorld()->GetTimerManager().ClearTimer(ClimbDetectionTimerHandle);

    // 计算翻越目标位置 (基于角色朝向 + DataAsset 偏移)
    FVector ClimbUpOffsetToUse = CharacterData ? CharacterData->ClimbUpOffset : FVector(80.0f, 0.0f, 70.0f);
    FVector TargetLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * ClimbUpOffsetToUse.X
        + OwnerCharacter->GetActorRightVector() * ClimbUpOffsetToUse.Y
        + FVector(0.0f, 0.0f, ClimbUpOffsetToUse.Z);
    FRotator TargetRot = FRotator(0.0f, OwnerCharacter->GetActorRotation().Yaw, 0.0f);

    // 设置翻越目标并切换到 ClimbUp 模式 (CMC 接管位移)
    CustomMovementComp->SetClimbUpTarget(TargetLoc, TargetRot);
    MovementComp->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::ClimbUp));

    // CMC 的 SetMovementMode 会触发 OnMovementModeChanged，自动广播事件，无需 FSM 中转

    if (EventClimbStopTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerCharacter, EventClimbStopTag, FGameplayEventData());
    }

    // 从 CharacterDataAsset 加载攀爬翻越蒙太奇
    if (CharacterData && !CharacterData->ClimbUpMontage.IsNull())
    {
        FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
        StreamableManager.RequestAsyncLoad(
            CharacterData->ClimbUpMontage.ToSoftObjectPath(),
            FStreamableDelegate::CreateLambda([this]()
            {
                if (bIsClimbingUp && CharacterData && !CharacterData->ClimbUpMontage.IsNull())
                {
                    UAnimMontage* LoadedMontage = CharacterData->ClimbUpMontage.Get();
                    if (LoadedMontage && OwnerCharacter)
                    {
                        USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
                        if (Mesh && Mesh->GetAnimInstance())
                        {
                            Mesh->GetAnimInstance()->OnMontageEnded.AddDynamic(this, &UClimbingComponent::OnClimbUpMontageEnded);
                        }
                        OwnerCharacter->PlayAnimMontage(LoadedMontage);
                    }
                }
            })
        );
        return; // 异步加载中，蒙太奇播放在回调中完成
    }

    // 没有蒙太奇：直接完成翻越
    FinishClimbUp();
}

void UClimbingComponent::OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 解绑委托，防止重复触发
    if (OwnerCharacter)
    {
        USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
        if (Mesh && Mesh->GetAnimInstance())
        {
            Mesh->GetAnimInstance()->OnMontageEnded.RemoveDynamic(this, &UClimbingComponent::OnClimbUpMontageEnded);
        }
    }

    // 无论正常结束还是被中断，都必须恢复状态
    FinishClimbUp();
}

void UClimbingComponent::FinishClimbUp()
{
    if (!bIsClimbingUp) return;

    bIsClimbingUp = false;

    // 恢复到下落模式
    if (MovementComp)
    {
        MovementComp->SetMovementMode(MOVE_Falling);
        MovementComp->bOrientRotationToMovement = true;
    }

    // CMC 的 SetMovementMode 会触发 OnMovementModeChanged，自动广播事件，无需 FSM 中转

    if (OwnerCharacter)
    {
        FRotator CurrentRot = OwnerCharacter->GetActorRotation();
        OwnerCharacter->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
    }

    CurrentWallNormal = FVector::ZeroVector;
    bCMCHitWall = false;
    CMCHitWallNormal = FVector::ZeroVector;
}

// --- 降频检测回调 ---

void UClimbingComponent::OnClimbDetectionTick()
{
    if (!OwnerCharacter || !MovementComp) return;

    // 使用 CMC 的状态查询替代 FSM (事件驱动，无 Tick 轮询)
    if (!CustomMovementComp || !CustomMovementComp->IsClimbing()) return;

    // 1. 检测攀爬转地面
    CheckClimbToGround();

    if (CustomMovementComp && !CustomMovementComp->IsClimbing()) return;

    // 2. 检测墙角过渡
    CheckCornerTransition();

    if (CustomMovementComp && CustomMovementComp->IsInCornerTransition()) return;

    // 3. 更新墙面法线
    if (bCMCHitWall && !CMCHitWallNormal.IsNearlyZero())
    {
        CurrentWallNormal = CMCHitWallNormal;
        if (CustomMovementComp)
        {
            CustomMovementComp->SetClimbWallNormal(CMCHitWallNormal);
        }
        bCMCHitWall = false;
        CMCHitWallNormal = FVector::ZeroVector;
    }
    else
    {
        FHitResult ChestHit, HeadHit;
        if (PerformClimbTraces(FVector::ZeroVector, ChestHit, HeadHit))
        {
            CurrentWallNormal = ChestHit.Normal;
            if (CustomMovementComp)
            {
                CustomMovementComp->SetClimbWallNormal(ChestHit.Normal);
            }
        }
    }

    // 4. 检测翻越
    CheckAndClimbUp();
}

void UClimbingComponent::OnFallingToClimbTick()
{
    if (!OwnerCharacter || !MovementComp) return;

    // 使用 CMC 的状态查询替代 FSM
    if (!CustomMovementComp || !CustomMovementComp->IsFalling())
    {
        GetWorld()->GetTimerManager().ClearTimer(FallingToClimbTimerHandle);
        return;
    }

    CheckFallingToClimb();
}

// --- 输入处理 (仅用于检测逻辑，不转发给 CMC) ---

void UClimbingComponent::OnInputDetectionTick()
{
    if (!MovementComp) return;

    // 从 CMC 的 LastInputVector 读取输入方向 (解耦：不依赖 APlayerCharacter 委托)
    FVector LastInput = MovementComp->GetLastInputVector();
    float InputX = LastInput.X;
    float InputY = LastInput.Y;

    HandleOwnerMovementInput(InputX, InputY);
}

void UClimbingComponent::HandleOwnerMovementInput(float InputX, float InputY)
{
    if (!MovementComp) return;

    if (bIsClimbingUp) return;

    // 使用 CMC 的状态查询替代 FSM (事件驱动，无 Tick 轮询)
    bool bIsFalling = CustomMovementComp && CustomMovementComp->IsFalling();
    bool bIsGrounded = CustomMovementComp && CustomMovementComp->IsGrounded();

        // 下落状态 → 启动/保持低频定时器
        if (bIsFalling)
    {
        if (!FMath::IsNearlyZero(InputX) || !FMath::IsNearlyZero(InputY))
        {
            if (!GetWorld()->GetTimerManager().IsTimerActive(FallingToClimbTimerHandle))
            {
                GetWorld()->GetTimerManager().SetTimer(
                    FallingToClimbTimerHandle,
                    this,
                    &UClimbingComponent::OnFallingToClimbTick,
                    FallingToClimbDetectionInterval,
                    true,
                    0.0f
                );
            }
        }
        else
        {
            GetWorld()->GetTimerManager().ClearTimer(FallingToClimbTimerHandle);
        }
        return;
    }

        // 地面状态 → 尝试攀爬
        if (bIsGrounded)
    {
        TryClimb();
        return;
    }

        // 攀爬状态 → 不需要拦截输入
    // 输入走引擎原生管线 (AddMovementInput → CMC ConsumeInputVector)
    }
