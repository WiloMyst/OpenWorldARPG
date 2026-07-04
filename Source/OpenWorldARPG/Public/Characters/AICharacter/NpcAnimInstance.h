// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGAnimInstance.h"
#include "NpcAnimInstance.generated.h"

/**
 * NPC 专属动画实例。处理 IK 盯防和叙事状态。
 */
UCLASS()
class OPENWORLDARPG_API UNPCAnimInstance : public UOpenWorldARPGAnimInstance
{
    GENERATED_BODY()

public:
    UNPCAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    // --- IK Look At ---

    /** 看向目标的世界坐标，驱动 Head/Spine IK */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|LookAt")
    FVector LookAtTargetLocation = FVector::ZeroVector;

    // --- Narrative ---

    /** 当前对话情绪状态 (0=Neutral, 1=Happy, 2=Angry, 3=Sad, 4=Surprised) */
    UPROPERTY(BlueprintReadOnly, Category = "AnimData|Narrative")
    uint8 CurrentEmotionState = 0;

private:
    FVector SnapshotLookAtTargetLocation = FVector::ZeroVector;
    uint8 SnapshotEmotionState = 0;
    bool bSnapshotHasLookAtTarget = false;
};
