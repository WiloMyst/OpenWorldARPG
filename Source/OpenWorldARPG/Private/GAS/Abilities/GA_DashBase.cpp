// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_DashBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Core/PlayerControllers/MainGamePlayerController.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/KismetMathLibrary.h"

UGA_DashBase::UGA_DashBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_DashBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedPlayer = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 状态 Tag 由 ActivationOwnedTags 管理（无敌帧 Tag 等）

    // 计算冲刺方向并应用位移
    ApplyDashMovement();
}

void UGA_DashBase::ApplyDashMovement()
{
    if (!CachedPlayer) return;

    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp) return;

    // 获取移动输入向量
    const FVector InputVector = MoveComp->GetLastInputVector();
    const bool bHasMovementInput = InputVector.SquaredLength() > MovementInputThreshold;

    // 计算冲刺方向
    FVector DashDirection;
    UAnimMontage* MontageToPlay;

    if (bHasMovementInput)
    {
        // 有移动输入：向移动方向冲刺
        DashDirection = InputVector.GetSafeNormal();
        MontageToPlay = DashMontage;
    }
    else
    {
        // 无移动输入：向角色后方闪避
        DashDirection = -CachedPlayer->GetActorForwardVector();
        MontageToPlay = BackDashMontage;
    }

    // 旋转角色朝向冲刺方向
    const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(DashDirection);
    CachedPlayer->SetActorRotation(TargetRotation);

    // 计算目标位置
    const FVector StartLocation = CachedPlayer->GetActorLocation();
    const FVector TargetLocation = StartLocation + DashDirection * DashDistance;

    // 播放蒙太奇（监听 BlendOut 用于过渡到 Sprint，监听 Completed/Interrupted 用于结束）
    if (MontageToPlay)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
        MontageTask->OnBlendOut.AddDynamic(this, &UGA_DashBase::OnMontageBlendOut);
        MontageTask->OnCompleted.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->ReadyForActivation();
    }

    // 创建 RootMotionSource 位移
    TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
    RMS->InstanceName = FName("Dash");
    RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
    RMS->Priority = 5;
    RMS->StartLocation = StartLocation;
    RMS->TargetLocation = TargetLocation;
    RMS->Duration = DashDuration;
    RMS->bRestrictSpeedToExpected = true;
    // 位移结束后速度归零，防止角色残留惯性
    RMS->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    RMS->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

    DashRMS_ID = MoveComp->ApplyRootMotionSource(RMS);
    bHasActiveRMS = true;

    // 设置定时器：RMS 完成后触发清理逻辑
    FTimerHandle DashFinishTimer;
    GetWorld()->GetTimerManager().SetTimer(
        DashFinishTimer,
        this,
        &UGA_DashBase::OnDashFinished,
        DashDuration,
        false
    );
}

void UGA_DashBase::OnMontageBlendOut()
{
    // 蒙太奇即将结束（BlendOut），检测是否应过渡到 Sprint
    TryTransitionToSprint();
}

void UGA_DashBase::TryTransitionToSprint()
{
    if (!CachedPlayer || bTransitioningToSprint) return;

    // 条件1：玩家是否仍然按住左 Shift？
    APlayerController* PC = CachedPlayer->GetController<APlayerController>();
    if (!PC || !PC->IsInputKeyDown(EKeys::LeftShift))
    {
        return;
    }

    // 条件2：玩家是否有非零的移动输入？
    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp || MoveComp->GetLastInputVector().SquaredLength() <= MovementInputThreshold)
    {
        return;
    }

    // 条件3：当前体力是否足够？
    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;
    const float CurrentStamina = ASC->GetNumericAttribute(UAS_Player::GetStaminaAttribute());
    if (CurrentStamina < SprintTransitionStaminaThreshold)
    {
        return;
    }

    // 所有条件满足，标记过渡中，发送 SprintStart 事件
    bTransitioningToSprint = true;

    if (SprintStartEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CachedPlayer, SprintStartEventTag, FGameplayEventData());
    }
}

void UGA_DashBase::OnDashFinished()
{
    if (!CachedPlayer) return;

    // 位移完成：清空速度
    if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
    {
        MoveComp->Velocity = FVector::ZeroVector;
    }

    bHasActiveRMS = false;

    // 如果正在过渡到 Sprint，不需要在这里 EndAbility（蒙太奇结束时会触发）
    // 否则正常结束
    if (!bTransitioningToSprint)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_DashBase::OnMontageFinished()
{
    // 蒙太奇播放完毕或被打断，结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_DashBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 安全移除 RootMotionSource（防止技能被打断时 RMS 残留导致角色持续位移）
    if (bHasActiveRMS && CachedPlayer)
    {
        if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
        {
            MoveComp->RemoveRootMotionSourceByID(DashRMS_ID);
        }
        bHasActiveRMS = false;
    }

    // 杀死异步监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
