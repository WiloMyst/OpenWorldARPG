// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameServer/RemotePlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ARemotePlayerCharacter::ARemotePlayerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // 远程玩家不参与本地控制，关闭输入与摄像机跟随
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    AutoPossessAI = EAutoPossessAI::Disabled;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = false;
        MoveComp->bUseControllerDesiredRotation = false;
        MoveComp->GravityScale = 0.0f;  // 位置由服务器权威驱动，禁用本地重力
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    }
}

void ARemotePlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const FVector Current = GetActorLocation();
    const FVector ToTarget = TargetLocation - Current;
    const float Dist = ToTarget.Size();

    if (Dist < 0.01f)
    {
        if (!Current.Equals(TargetLocation, 0.01f))
        {
            SetActorLocation(TargetLocation);
        }
    }
    else
    {
        // 指数逼近 + 单帧位移上限，平滑且防穿墙
        const float Step = FMath::Min(Dist * InterpSpeed * DeltaTime, MaxStepPerFrame);
        SetActorLocation(Current + ToTarget.GetSafeNormal() * Step);
    }

    // 朝向平滑旋转
    const FRotator CurrentRot = GetActorRotation();
    const float DeltaYaw = FRotator::NormalizeAxis(TargetYaw - CurrentRot.Yaw);
    if (FMath::Abs(DeltaYaw) > 0.1f)
    {
        const float MaxYawStep = YawInterpSpeed * DeltaTime;
        const float NewYaw = CurrentRot.Yaw + FMath::Clamp(DeltaYaw, -MaxYawStep, MaxYawStep);
        SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));
    }
}

void ARemotePlayerCharacter::SetTargetTransform(const FVector& Location, float Yaw)
{
    TargetLocation = Location;
    TargetYaw = Yaw;
}

void ARemotePlayerCharacter::SnapTo(const FVector& Location, float Yaw)
{
    TargetLocation = Location;
    TargetYaw = Yaw;
    SetActorLocation(Location);
    SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
}
