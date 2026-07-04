// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/PlayerCharacter/PlayerAnimInstance.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/MovementSystem/Components/PlayerCharacterMovementComponent.h"
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

    // ================================================================
    // GameThread 快照：所有非线程安全的读取必须在此完成
    // ================================================================
    // 基类 NativeUpdateAnimation 已快照通用物理数据
    // （Velocity/ActorRotation/ActorLocation/ForwardVector/RightVector/LastInputVector/IsGrounded/IsFalling）。
    // 此处只补充玩家专属数据：Controller 旋转、CMC 运动状态、脚部骨骼位置。

    // --- 瞄准数据快照 ---
    const APlayerController* PC = CachedPlayerController.Get();
    if (PC)
    {
        SnapshotControlRotation = PC->GetControlRotation();
    }

    // --- CMC 状态快照（IsSprinting 等非线程安全，必须主线程读取）---
    const UPlayerCharacterMovementComponent* MoveComp = CachedMovementComp.Get();
    if (MoveComp)
    {
        bSnapshotIsSprinting = MoveComp->IsSprinting();
        bSnapshotIsWalking = MoveComp->IsWalking();
        bSnapshotIsAiming = MoveComp->IsAiming();
        bSnapshotIsClimbing = MoveComp->IsClimbing();
        bSnapshotIsGliding = MoveComp->IsGliding();
        bSnapshotIsSwimming = MoveComp->IsSwimming();
        bSnapshotIsFastSwimming = MoveComp->IsFastSwimming();
        SnapshotMaxSpeed = MoveComp->GetMaxSpeed();
    }

    // --- 脚部骨骼位置快照 ---
    if (USkeletalMeshComponent* MeshComp = GetSkelMeshComponent())
    {
        SnapshotLeftFootLoc = MeshComp->GetSocketLocation(LeftFootBoneName);
        SnapshotRightFootLoc = MeshComp->GetSocketLocation(RightFootBoneName);
    }
}

void UPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // 基类先更新通用数据（GroundSpeed/VelocityZ/bIsGrounded/bIsFalling 等）
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    // bIsMoving：玩家需要速度超过阈值且有输入加速度
    bIsMoving = (GroundSpeed > MoveSpeedThreshold) && !SnapshotLastInputVector.IsNearlyZero(0.01f);

    // ================================================================
    // Worker Thread 纯数据计算
    // ================================================================
    // 严禁出现任何 Character->Get...() 或 MoveComp->...() 调用。
    // 所有数据来源均为 NativeUpdateAnimation 中写入的 Snapshot... 变量
    // 或基类已快照的通用物理数据。

    // --- 1. 瞄准数据 (AimPitch / AimYaw) ---
    // 使用基类快照 SnapshotActorRotation 和本类快照 SnapshotControlRotation

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

    // --- 2. 玩家专属移动状态（从主线程快照读取）---

    bIsSprinting = bSnapshotIsSprinting;
    bIsWalking = bSnapshotIsWalking;
    bIsAiming = bSnapshotIsAiming;
    bIsClimbing = bSnapshotIsClimbing;
    bIsGliding = bSnapshotIsGliding;
    bIsSwimming = bSnapshotIsSwimming;
    bIsFastSwimming = bSnapshotIsFastSwimming;

    // --- 3. 输入数据 ---
    // 使用基类快照 SnapshotLastInputVector 和 SnapshotActorRotation

    AccelerationVector = SnapshotLastInputVector.GetSafeNormal(0.0001f);
    if (!SnapshotLastInputVector.IsNearlyZero(0.01f))
    {
        const FVector LocalInput = SnapshotActorRotation.UnrotateVector(SnapshotLastInputVector);
        InputX = LocalInput.X;
        InputY = LocalInput.Y;
    }
    else
    {
        InputX = 0.0f;
        InputY = 0.0f;
    }

    // --- 4. Update Velocity (插值计算 Speed X 和 Speed Y) ---

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
    else if (bIsSwimming)
    {
        // 游泳状态：使用本地空间输入方向驱动混合空间
        const FVector LocalAcceleration = SnapshotActorRotation.UnrotateVector(AccelerationVector);

        float TargetX = LocalAcceleration.Y * 100.0f; // 左右
        float TargetY = LocalAcceleration.X * 100.0f; // 前后

        SpeedX = FMath::FInterpTo(SpeedX, TargetX, DeltaSeconds, 5.0f);
        SpeedY = FMath::FInterpTo(SpeedY, TargetY, DeltaSeconds, 5.0f);
    }
    else
    {
        // 行走状态：基于实际加速度和最大速度的方向投影
        FVector TargetVelocity = AccelerationVector * SnapshotMaxSpeed;

        DirectionCurrent = FMath::VInterpTo(DirectionCurrent, TargetVelocity, DeltaSeconds, 5.0f);

        // 使用基类快照的方向向量
        SpeedX = FVector::DotProduct(DirectionCurrent, SnapshotActorRightVector);
        SpeedY = FVector::DotProduct(DirectionCurrent, SnapshotActorForwardVector);
    }

    // --- 5. Update Step Stop State ---

    // 使用带有容差的相对速度判断
    const bool bHasRelativeSpeed = (GroundSpeed > 5.0f);
    const bool bHasInput = !AccelerationVector.IsNearlyZero();
    const bool bPrevInStepStopping = bInStepStopping;

    // --- 外力抗性逻辑 ---
    if (bHasInput)
    {
        // 只要有输入，绝对不是急停
        bInStepStopping = false;
    }
    else
    {
        // 没有输入时，判断是"玩家刚松手"还是"被外力推着走"
        if (bHadInputLastFrame && bHasRelativeSpeed && bIsGrounded)
        {
            // 玩家上一帧有输入，这帧松手了，且有残余速度，合法进入急停
            bInStepStopping = true;
        }
        else if (bInStepStopping && bHasRelativeSpeed && bIsGrounded)
        {
            // 已经在急停状态中，且速度还没降到阈值以下，继续保持急停
            bInStepStopping = true;
        }
        else
        {
            // 速度降下来了，或者根本不是玩家主动起步的（单纯被外力推挤），不触发急停
            bInStepStopping = false;
        }
    }

    // --- 状态机快照：当且仅当刚刚进入急停状态的那一帧执行 ---
    if (bInStepStopping && !bPrevInStepStopping)
    {
        // 捕获最高速度，后续减速期间不再被覆盖
        SpeedOnStop = GroundSpeed;

        // --- 意图继承与大角度转身判定 ---
        if (!SnapshotLastInputVector.IsNearlyZero())
        {
            // 计算玩家最后意图方向的旋转
            const FRotator IntentRot = SnapshotLastInputVector.Rotation();

            // 计算当前角色朝向与意图朝向的角度差（自动归一化到 -180 到 +180）
            StopYawDelta = FMath::FindDeltaAngleDegrees(SnapshotActorRotation.Yaw, IntentRot.Yaw);

            // 判断是否大于 90 度（大角度转身）
            if (FMath::Abs(StopYawDelta) > 90.0f)
            {
                bIsTurnStop = true;
                bTurnStopRight = (StopYawDelta > 0.0f); // 正数为右转，负数为左转
            }
            else
            {
                bIsTurnStop = false;
            }
        }
        else
        {
            StopYawDelta = 0.0f;
            bIsTurnStop = false;
        }

        // --- 计算停步时是左脚还是右脚在前 ---
        const FVector RootLoc = SnapshotActorLocation;
        const FVector ForwardDir = SnapshotActorForwardVector;

        const float LeftForwardDist = FVector::DotProduct(SnapshotLeftFootLoc - RootLoc, ForwardDir);
        const float RightForwardDist = FVector::DotProduct(SnapshotRightFootLoc - RootLoc, ForwardDir);

        bStopOnLeftFoot = (LeftForwardDist > RightForwardDist);
    }
    else if (bHasInput)
    {
        // 【防提前归零修复】：只有当玩家再次推摇杆时，才重置速度缓存
        SpeedOnStop = 0.0f;

        // 清理转身状态
        bIsTurnStop = false;
        StopYawDelta = 0.0f;
    }

    // 记录本帧的输入状态，供下一帧判定使用
    bHadInputLastFrame = bHasInput;

    bShouldGroundMove2StopStep = (bInStepStopping && !bIsAiming);
    bShouldStopStep2GroundMove = (bIsGrounded && bIsMoving);
    bShouldAirborne2GroundMove = (bIsGrounded && bIsMoving);
    bShouldGroundMove2JumpStart = (VelocityZ > 30.0f);
    bShouldGroundMove2FallLoop = (VelocityZ <= 30.0f);
    bShouldJumpStart2FallLoop = (VelocityZ <= 200.0f);
}
