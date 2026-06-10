// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/PlayerAnimInstance.h"
#include "Characters/PlayerCharacter.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UPlayerAnimInstance::UPlayerAnimInstance()
    : Super()
{
}

void UPlayerAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

        // 额外缓存玩家专属指针
    
    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(TryGetPawnOwner()))
    {
        CachedPlayerCharacter = PlayerChar;

        // 缓存 PlayerController（用于读取 ControlRotation 计算 AimOffset）
        if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
        {
            CachedPlayerController = PC;
        }
    }
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // GameThread 快照：所有非线程安全的读取必须在此完成
    const ACharacter* Character = CachedCharacter.Get();
    if (Character)
    {
        SnapshotActorRotation = Character->GetActorRotation();
        SnapshotActorLocation = Character->GetActorLocation();
        SnapshotActorForwardVector = Character->GetActorForwardVector();
        SnapshotActorRightVector = Character->GetActorRightVector();
    }

    const APlayerController* PC = CachedPlayerController.Get();
    if (PC)
    {
        SnapshotControlRotation = PC->GetControlRotation();
    }

    if (USkeletalMeshComponent* MeshComp = GetSkelMeshComponent())
    {
        SnapshotLeftFootLoc = MeshComp->GetSocketLocation(LeftFootBoneName);
        SnapshotRightFootLoc = MeshComp->GetSocketLocation(RightFootBoneName);
    }
}

void UPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // 基类先更新通用数据
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    const ACharacter* Character = CachedCharacter.Get();
    const UOpenWorldARPGCharacterMovementComponent* MoveComp = CachedMovementComp.Get();

    if (!Character || !MoveComp)
    {
        return;
    }

    // 1. 瞄准数据 (AimPitch / AimYaw)
    // 使用 GameThread 快照，避免在工作线程访问非线程安全的 Controller/Actor 状态
    
    const FRotator ControlRotation = SnapshotControlRotation;
    const FRotator ActorRotation = SnapshotActorRotation;

    if (!ControlRotation.IsZero() || !ActorRotation.IsZero())
    {
        // 差值旋转：将 Controller 旋转转换到角色本地空间
        const FRotator DeltaRotation = (ControlRotation - ActorRotation).GetNormalized();

        AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0, 90.0f);
        AimYaw = DeltaRotation.Yaw;

        // SpineRotation：用于上半身扭转动画
        SpineRotation = FRotator(
            FMath::Clamp(AimPitch * 0.5f, -45.0f, 45.0f),
            FMath::Clamp(AimYaw * 0.6f, -60.0f, 60.0f),
            0.0f
        );
    }
    else
    {
        AimPitch = 0.0f;
        AimYaw = 0.0f;
        SpineRotation = FRotator::ZeroRotator;
    }

        // 2. 玩家专属移动状态 (从 CMC 读取)
    
    bIsSprinting = MoveComp->IsSprinting();
    bIsWalking = MoveComp->IsWalking();
    bIsAiming = MoveComp->IsAiming();
    bIsClimbing = MoveComp->IsClimbing();
    bIsGliding = MoveComp->IsGliding();

        // 4. 输入数据
    // 从 CMC 的 GetLastInputVector 推算本地空间输入方向
    // 使用 GameThread 快照的 ActorRotation，避免工作线程访问非线程安全数据
    
    const FVector LastInput = MoveComp->GetLastInputVector();
    AccelerationVector = LastInput.GetSafeNormal(0.0001f);
    if (!LastInput.IsNearlyZero(0.01f))
    {
        const FVector LocalInput = SnapshotActorRotation.UnrotateVector(LastInput);
        InputX = LocalInput.X;
        InputY = LocalInput.Y;
    }
    else
    {
        InputX = 0.0f;
        InputY = 0.0f;
    }

        // Update Velocity (插值计算 Speed X 和 Speed Y)
    // 必须在急停判定前计算，因为插值依赖 DeltaSeconds
    if (bIsClimbing)
    {
        // 攀爬状态：取出归一化的输入方向，转换到本地空间
        const FVector LocalAcceleration = SnapshotActorRotation.UnrotateVector(AccelerationVector);

        // 使用 Y 和 Z 轴驱动
        float TargetX = LocalAcceleration.Y * 100.0f; // 左右
        float TargetY = LocalAcceleration.Z * 100.0f; // 上下

        SpeedX = FMath::FInterpTo(SpeedX, TargetX, DeltaSeconds, 5.0f);
        SpeedY = FMath::FInterpTo(SpeedY, TargetY, DeltaSeconds, 5.0f);
    }
    else
    {
        // 行走状态：基于实际加速度和最大速度的方向投影
        FVector TargetVelocity = AccelerationVector * MoveComp->GetMaxSpeed();
        
        DirectionCurrent = FMath::VInterpTo(DirectionCurrent, TargetVelocity, DeltaSeconds, 5.0f);

        // 使用快照的方向向量，避免工作线程访问非线程安全数据
        SpeedX = FVector::DotProduct(DirectionCurrent, SnapshotActorRightVector);
        SpeedY = FVector::DotProduct(DirectionCurrent, SnapshotActorForwardVector);
    }

    // Update Step Stop State
    // 给 GroundSpeed 加一个极小的阈值（如 5.0f），防止物理浮点数漂移导致状态反复横跳
    const bool bHasSpeed = (GroundSpeed > 5.0f); 
    const bool bNoInput = AccelerationVector.IsNearlyZero();

    // 1. 记录上一帧的状态（核心逻辑：边缘检测）
    const bool bPrevInStepStopping = bInStepStopping;

    // 2. 更新当前急停状态标志
    bInStepStopping = (bHasSpeed && bNoInput && !bIsFalling);

    // 3. 状态机判断：当且仅当刚刚进入急停状态的那一帧执行
    if (bInStepStopping && !bPrevInStepStopping)
    {
        // 捕获最高速度，且后续减速期间不再被覆盖
        SpeedOnStop = GroundSpeed;

        // 计算停步时是左脚还是右脚在前
        if (Character)
        {
            const FVector RootLoc = SnapshotActorLocation;
            const FVector ForwardDir = SnapshotActorForwardVector;

            // 将脚部位置向量与角色面朝前向向量做点乘 (Dot Product)
            // 结果越大，说明这只脚在角色面朝方向上越靠前
            const float LeftForwardDist = FVector::DotProduct(SnapshotLeftFootLoc - RootLoc, ForwardDir);
            const float RightForwardDist = FVector::DotProduct(SnapshotRightFootLoc - RootLoc, ForwardDir);

            // 如果左脚的投影距离大于右脚，说明左脚在前
            bStopOnLeftFoot = (LeftForwardDist > RightForwardDist);
        }
    }
    else if (!bInStepStopping)
    {
        // 恢复正常状态逻辑保持不变
        if (!bCanSetSpeedOnStep)
        {
            SpeedOnStop = 0.0f;
        }
    }
}
