// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Animation/PlayerAnimInstance.h"
#include "Characters/PlayerCharacter.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Components/TargetSelectionComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"

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

        // 缓存目标选取组件（用于读取锁定状态）
        CachedTargetSelectionComp = PlayerChar->FindComponentByClass<UTargetSelectionComponent>();
    }
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

        // GameThread 快照：目标锁定状态
    // TargetSelectionComponent 的 GetBestTarget 不是线程安全的
    
    bSnapshotTargetLocking = false;
    if (CachedTargetSelectionComp.IsValid())
    {
        bSnapshotTargetLocking = (CachedTargetSelectionComp->GetBestTarget() != nullptr);
    }

    // 拍下脚部位置快照供工作线程使用
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
    //
    // 计算逻辑：Controller ControlRotation - Actor Rotation
    // Controller 旋转由玩家输入在 GameThread 驱动
    // Worker Thread 读取时可能有一帧延迟，但对动画表现完全可接受
    
    const APlayerController* PC = CachedPlayerController.Get();
    if (PC)
    {
        const FRotator ControlRotation = PC->GetControlRotation();
        const FRotator ActorRotation = Character->GetActorRotation();

        // 差值旋转：将 Controller 旋转转换到角色本地空间
        const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(ControlRotation, ActorRotation);

        AimPitch = FMath::Clamp(DeltaRotation.Pitch, -90.0f, 90.0f);
        AimYaw = DeltaRotation.Yaw;

        // SpineRotation：用于上半身扭转动画
        // 将 AimYaw 按比例映射到脊椎旋转范围（通常 ±45 度）
        SpineRotation = FRotator(
            FMath::Clamp(AimPitch * 0.5f, -45.0f, 45.0f),  // Pitch 映射到脊椎前倾/后仰
            FMath::Clamp(AimYaw * 0.6f, -60.0f, 60.0f),     // Yaw 映射到脊椎扭转
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

        // 3. 目标锁定状态 (主线程快照)
    
    bIsTargetLocking = bSnapshotTargetLocking;

        // 4. 输入数据
    // 从 CMC 的 GetLastInputVector 推算本地空间输入方向
    // 不依赖 APlayerCharacter 的 protected 成员，保持解耦
    
    const FVector LastInput = MoveComp->GetLastInputVector();
    AccelerationVector = LastInput.GetSafeNormal(0.0001f);
    if (!LastInput.IsNearlyZero(0.01f))
    {
        const FVector LocalInput = Character->GetActorRotation().UnrotateVector(LastInput);
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
        const FVector LocalAcceleration = Character->GetActorRotation().UnrotateVector(AccelerationVector);

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

        // 使用点乘 (Dot Product) 提取相对于角色朝向的前后/左右速度分量
        SpeedX = FVector::DotProduct(DirectionCurrent, Character->GetActorRightVector());
        SpeedY = FVector::DotProduct(DirectionCurrent, Character->GetActorForwardVector());
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
        if (Character) // 确保前面已经获取了 CachedCharacter.Get()
        {
            const FVector RootLoc = Character->GetActorLocation();
            const FVector ForwardDir = Character->GetActorForwardVector();

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
